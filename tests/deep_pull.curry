f [_:_:_:a:_]
    | a==1 = True
    | otherwise = False

main = [1,2,3,0?1,4,5,6]

------ CORRECT ANSWER BELOW GENERATED BY cytest.py ------
--> [1,2,3,0,4,5,6]
--> [1,2,3,1,4,5,6]
