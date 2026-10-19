               JOB  T06 BRANCH TESTS, WORD MARKS, MISCELLANY
     ******************************************************************
     * T06 - BWZ, BBE, BCE, BW, BM, CLEAR WORD MARK, NOP, HALT AND
     *       BRANCH, CHAINED MOVES, MCW TO A WORD MARK IN B, MOVE
     *       NUMERIC/ZONE CHAINING, CLEAR STORAGE AND BRANCH.
     ******************************************************************
               ORG  400
     START     CS   332
               CS
               BWZ  T1,MINUS,K
               MCW  @NOK1@,204
               B    T2
     T1        MCW  @OK1@,203
     T2        BW   T3,WMF
               MCW  @NOK2@,208
               B    T4
     T3        MCW  @OK2@,207
     T4        BBE  T5,CHB,1
               MCW  @NOK3@,212
               B    T6
     T5        MCW  @OK3@,211
     T6        BCE  T7,CHB,A
               MCW  @NOK4@,216
               B    T8
     T7        MCW  @OK4@,215
     T8        BM   T9,MINUS
               MCW  @NOK5@,220
               B    T10
     T9        MCW  @OK5@,219
     T10       CW   WMF
               BW   T11,WMF
               MCW  @OK6@,223
               B    T12
     T11       MCW  @NOK6@,224
     T12       NOP  1,2
               MCW  F3,230
               MCW
               MCW
               MCW  @AB@,F1
               MN   @12@,F1
               MN
               MZ   @J@,F1
               MCW  F1,240
               W
               CS   332
               CS
               SW   250
               MCW  @XXXXXXXXXXXX@,255
               MCW  @HELLO@,256
               W
               CS   T13,332
               H    0
     T13       CS   299
               MCW  @AFTER CS AND BRANCH@,220
               W
               H    START
     MINUS     DCW  -5
     WMF       DCW  @W@
     CHB       DCW  @A@
     F1        DCW  @..@
               DCW  @PQRS@
               DCW  @TUV@
     F3        DCW  @W@
               END  START
