f x y = x == let z = y:z in length z

main = f 2 [1]

------ CORRECT ANSWER BELOW GENERATED BY cytest.py using pakcs ------
--$?-> 1