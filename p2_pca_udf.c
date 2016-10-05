#include "postgres.h"
#include "utils/builtins.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "executor/spi.h"
#include "lib/stringinfo.h"
#include "fmgr.h"
#include <stdlib.h>
#include <string.h>
#include "lapacke.h"

#define min(a,b) ((a)>(b)?(b):(a))
#define COMMA ","

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

//pca variables
int32 i, j;
int32 a, b, n, d; // indices for LQ
int32 rows; // n
int32 cols; // d
int32 m, n, lda, ldu, ldvt, info; // lapack svd variables
int32 * columns_idx;
char * table_name;
char * columns_names;
char * select_query;
char * column_token;
double * X; // d-vector
double * L; // d-vector
double ** Q; //Q triangular dd-matrix
double ** P; //p dd-matrix (correlation matrix)
double * A; //input matrix for lapack (correlation matrix)

// postgres variables
StringInfo si;
char * tmp_value;
text * solution;
TupleDesc tupdesc;
SPITupleTable * tuptable;
HeapTuple tuple;
char * command;

PG_FUNCTION_INFO_V1(pca);
Datum
pca(PG_FUNCTION_ARGS)
{
	// returns null on null input
	if (PG_ARGISNULL(0) || PG_ARGISNULL(1)) { PG_RETURN_NULL(); }

	si = makeStringInfo();
	// gets table name
	table_name = text_to_cstring(PG_GETARG_TEXT_P(0));
	//gets columns to take from table
	columns_names = text_to_cstring(PG_GETARG_TEXT_P(1));
	// counts columns separated by commas
	for (j=0, cols=1; columns_names[j]; j++) { cols += (columns_names[j] == ','); }

	//connects to Postgres if the number of columns is greater than 1
	if (cols > 1 && SPI_connect() == SPI_OK_CONNECT) {
		//allocates memory for columns indexes
		columns_idx = (int32*) palloc (cols * sizeof(int32));
		//allocates memory and creates a select query to retrieve data from the indicated table
		select_query = (char*) palloc (strlen(table_name) + strlen(columns_names) + 24 * sizeof(char));
		sprintf(select_query, "SELECT %s FROM %s ", columns_names,  table_name);
		// allocates memory for PCA variables (initialized with zero)
		X = (double*) palloc (cols * sizeof(double));
		L = (double*) palloc (cols * sizeof(double));
		Q = (double**) palloc (cols * sizeof(double*));
		P = (double**) palloc (cols * sizeof(double*));
		A = (double*) palloc (cols * cols * sizeof(double));
		memset(X, 0, cols * sizeof(double));
		memset(L, 0, cols * sizeof(double));
		for (j=0; j<cols; j++) {
			Q[j] = (double*) palloc (cols * sizeof(double));
			P[j] = (double*) palloc (cols * sizeof(double));
			memset(Q[j], 0, cols * sizeof(double));
			memset(P[j], 0, cols * sizeof(double));
		}

		//executes select query
		if (SPI_execute(select_query, true, 0) == SPI_OK_SELECT && SPI_tuptable != NULL) {
			//retrieves data set
			rows = SPI_processed;
			tupdesc = SPI_tuptable->tupdesc;
			tuptable = SPI_tuptable;

			//stores column indexes
			j = 0;
			column_token = strtok(columns_names, COMMA);
			while(column_token) {
				columns_idx[j++] = SPI_fnumber(tupdesc, column_token);
				column_token = strtok(NULL, COMMA);
			}
			//scans the table
			for (i = 0; i < rows; i++) {
				// gets next tuple
				tuple = tuptable->vals[i];
				for (j=0; j<cols; j++) {
					// gets j-column
					tmp_value = SPI_getvalue(tuple, tupdesc , columns_idx[j]);
					//we know each value has to be float
					X[j] = atof(tmp_value);
					pfree(tmp_value);
				} //ends for j

				// aggregates X
				for(a=0; a<cols; a++) {
					L[a] += X[a];
					for(b=0; b<=a; b++) {
						Q[a][b] += X[a]*X[b]; //Q[b][a] = Q[a][b]
					} // ends for b
				} //ends for a

			} //ends for i (ends scan table)

			// Now L and Q are computed and allocated in RAM
			// and P (correlation matrix) has to be calculated
			n = rows;
			d = cols;
			for(a=0; a<cols; a++) {
				// populates P
				for(b=0; b<=a; b++) {
					P[a][b] = (n*Q[a][b] - L[a]*L[b]) / (sqrt(n*Q[a][a] - L[a]*L[a]) * sqrt(n*Q[b][b] - L[b]*L[b]));
					P[b][a] = P[a][b]; // necessary ?
				} // ends for b
			} //ends for a

			// populates A, A is P but linearized (lapack requires it this way)
			for(a=0; a<cols; a++) {
				for(b=0; b<cols; b++) {
					A[a*d + b] = P[a][b]; // necessary?, A can be directly populated (without P)
				} // ends for b
			} //ends for a

			// We can pass P to LAPACK and use SVD
			m = cols;
			n = cols;
			lda = m;
			ldu = m;
			ldvt = n;
			double s[n], u[ldu*m], vt[ldvt*n];
			double superb[min(m,n)-1];
			// computes SVD
			info = LAPACKE_dgesvd( LAPACK_ROW_MAJOR, 'A', 'A', m, n, A, lda, s, u, ldu, vt, ldvt, superb );

			// gets principal components (solution)
			//--------------------
			appendStringInfo(si, "correlation matrix: "); //TODO remove this line
			for(a=0; a<cols; a++) {
				for(b=0; b<cols; b++) {
					appendStringInfo(si, "%6.2f ", P[a][b]); //TODO remove this line
				} // ends for b
				appendStringInfo(si, "|"); //TODO remove this line
			} //ends for a
			appendStringInfo(si, "$"); //TODO remove this line
			//--------------------
			appendStringInfo(si, "[S] singular values: "); //TODO remove this line
			for( i = 0; i < n; i++ ) {
				appendStringInfo(si, "%6.2f ", s[i]); //TODO remove this line
			} //ends for i
			appendStringInfo(si, "$"); //TODO remove this line
			//--------------------
			appendStringInfo(si, "deviation standard: "); //TODO remove this line
			for( i = 0; i < n; i++ ) {
				appendStringInfo(si, "%6.2f ", sqrt(s[i])); //TODO remove this line
			} //ends for i
			appendStringInfo(si, "$"); //TODO remove this line
			//--------------------
			appendStringInfo(si, "[U] Left singular vectors (stored column-wise): ");
			for( i = 0; i < m; i++ ) {
				for( j = 0; j < n; j++ ) {
						appendStringInfo(si, "%6.2f ", u[i+j*ldu]); //TODO remove this line
				} //ends for j
				appendStringInfo(si, "|"); //TODO remove this line
			} //ends for i
			appendStringInfo(si, "$"); //TODO remove this line
			//--------------------
			appendStringInfo(si, "[Vtransposed] Right singular vectors (stored row-wise): "); //TODO remove this line
			for( i = 0; i < n; i++ ) {
				for( j = 0; j < n; j++ ) {
						appendStringInfo(si, "%6.2f ", vt[i+j*ldvt]); //TODO remove this line
				} //ends for j
				appendStringInfo(si, "|"); //TODO remove this line
			} //ends for i
			appendStringInfo(si, "$"); //TODO remove this line


		} // ends SPI_execute select query

		//closes db connection
		SPI_finish();
		//frees memory
		if(select_query) { pfree(select_query); }
		if(columns_idx) { pfree(columns_idx); }
		if(X) { pfree(X); }
		if(L) { pfree(L); }
		for (j=0; j<cols; j++) {
			if(Q[j]) { pfree(Q[j]); }
			if(P[j]) { pfree(P[j]); }
		}
		if(Q) { pfree(Q); }
		if(P) { pfree(P); }
		if(A) { pfree(A); }
	} // ends if-connect

	//frees memory
	if(table_name) { pfree(table_name); }
	if(columns_names) { pfree(columns_names); }

	//returns solution
	solution = (text *) cstring_to_text_with_len(si->data , si->len);
	PG_RETURN_TEXT_P (solution); //do we really want to return a string?, no we do not

}
