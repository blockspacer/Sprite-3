sign a = let c0 = a<0
             c1 = a>0
           in if c0 then -1 else if c1 then 1 else 0
main = sign (-1 ? 0 ? 1)

------ CORRECT ANSWER BELOW GENERATED BY cytest.py ------
--> (-1)
--> 0
--> 1
