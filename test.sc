use test.dll as test
call example
call test1_func abc 123 #2 arguments are provided
use ./other.dll as test2
call test2.func1 \
def456 # this line should belong to above line
call test.example 
rem test2
#Expect error in below command
call test2.func1