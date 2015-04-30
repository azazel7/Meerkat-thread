echo "=====Test join====="
./build/11-join
echo "=====Test join-main====="
./build/12-join-main
echo "=====Test create-many====="
./build/21-create-many 1000
echo "=====Test create-many-recursive====="
./build/22-create-many-recursive 1000
echo "=====Test create-many-once====="
./build/23-create-many-once 1000
echo "=====Test switch-many====="
./build/31-switch-many 500 500
echo "=====Test switch-many-join====="
./build/32-switch-many-join 500 500
echo "=====Test fibonacci====="
./build/51-fibonacci 25

