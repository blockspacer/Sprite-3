data A = A Bool | B
main = cond (x=:=(A y) & y=:=False) x where x,y free

------ CORRECT ANSWER BELOW GENERATED BY cytest.py using pakcs ------
--> A False
--$?-> 0