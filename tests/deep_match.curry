data Rank = One | Two
data Suit = Clubs | Spades
data Card = Card Rank Suit

guess (Card Two Spades) = True


main = guess (Card Two (Spades?Clubs))


------ CORRECT ANSWER BELOW GENERATED BY cytest.py using pakcs ------
--> True
--$?-> 0