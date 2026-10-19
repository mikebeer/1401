               JOB  HELLO FROM THE IBM 1401
     ******************************************************************
     * HELLO - THE FIRST PROGRAM: CLEAR THE PRINT AREA, MOVE A LINE
     *         INTO IT, PRINT IT, HALT.
     ******************************************************************
               ORG  333
     START     CS   332
               CS
               MCW  @HELLO, WORLD - FROM THE IBM 1401 ON VM/370PLUS@,250
               W
               H    START
               END  START
