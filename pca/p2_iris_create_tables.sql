-- creates a table for 4 numeric, predictive attributes and the plant class
CREATE TABLE iris_plants (
sepal_length float,
sepal_width float,
petal_length float,
petal_width float,
iris_class varchar(32)
);

-- loads data
COPY iris_plants FROM '/tmp/iris.data.txt' WITH DELIMITER ',' NULL '';

-- creates a primary key
ALTER TABLE iris_plants ADD COLUMN id_plant serial primary key;

-- creates normalized columns
alter table iris_plants add column nsl float; -- normalized sepal_length
alter table iris_plants add column nsw float; -- normalized sepal_width
alter table iris_plants add column npl float; -- normalized petal_length
alter table iris_plants add column npw float; -- normalized petal_width

update iris_plants set nsl = t2.nsl, nsw = t2.nsw, npl = t2.npl, npw = t2.npw
from (
	select id_plant
	, (sepal_length - avg (sepal_length) over ( )) / (stddev (sepal_length) over ()) as nsl
	, (sepal_width - avg (sepal_width) over ( )) / (stddev (sepal_width) over ()) as nsw
	, (petal_length - avg (petal_length) over ( )) / (stddev (petal_length) over ()) as npl
	, (petal_width - avg (petal_width) over ( )) / (stddev (petal_width) over ()) as npw
	from iris_plants
) as t2
where iris_plants.id_plant = t2.id_plant

-- shows data
select * from iris_plants
