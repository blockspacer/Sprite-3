xor :: Bool -> Bool -> Bool
xor True True = False
xor True False = True
xor False True = True
xor False False = False
main :: Bool
main = cond ((x=:=y) ? success) (xor x y) where x,y free

------ CORRECT ANSWER BELOW GENERATED BY cytest.py ------
--> False
--> False
--> False
--> False
--> True
--> True
