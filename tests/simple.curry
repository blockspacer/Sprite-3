myTrue = True

myOr True _ = True
myOr False x = x

main = myOr True False

------ CORRECT ANSWER BELOW GENERATED BY cytest.py ------
--> True