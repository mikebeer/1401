               JOB  T13 MACROS, CALL AND INCLD
     ******************************************************************
     * T13 - LIBRARY MACROS (IN TESTS/MAC): OPERANDS, THE LABEL, A
     *       LITERAL OPERAND WITH A COMMA, INTERNAL LABELS, STATEMENTS
     *       DROPPED FOR A MISSING OR PRESENT OPERAND, A CONTINUED
     *       OPERAND LIST, NESTED MACROS, CALL AND INCLD.
     ******************************************************************
               ORG  500
     X         DCW  005
     Y         DCW  100
     Z         DCW  200
     START     CS   332
               CS
     FIRST     PRCON @HELLO, WORLD@,212
               PRCON @SECOND@,206
               ADD2 X,Y,Z
               ADD2 X,Y
               ADD2 X,
                    Z
               TWICE @TWICE@
               CALL PRSUB
               MCW  TBL1,209
               W
               MCW  TBL2,209
               W
               B    FIRST2
     FIRST2    H    START
               INCLD TBL
               END  START
