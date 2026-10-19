               JOB  T12 RELOCATABLE LOCATION COUNTERS
     ******************************************************************
     * T12 - RELOCATABLE LOCATION COUNTERS, PLACED BY THE ASSEMBLER:
     *       LC 2,R AFTER THE HIGHEST ABSOLUTE ADDRESS, LC 3,R,X00+6 AT
     *       THE NEXT HUNDRED PLUS 6, LC 4,X,TABLE+20 AT A LABEL.
     ******************************************************************
               ORG  500
     START     CS   332
               CS
               B    SUB
     BACK      MCW  MSG2,220
               W
               CS   332
               CS
               SBR  TABLE-6,MSG1
               SBR  TABLE-3,MSG2
               SBR  TABLE,T4
               SW   201
               MCW  TABLE,209
               W
               H    START
               LC   2,R
     MSG1      DCW  @HELLO@
     SUB       MCW  MSG1,215
               W
               B    BACK
               LC   3,R,X00+6
     MSG2      DCW  @WORLD@
               LC   4,X,TABLE+20
     T4        DCW  @T4@
               LC   0
               ORG  2000
     TABLE     DS   10
               END  START
