               JOB  T11 ABSOLUTE LOCATION COUNTERS
     ******************************************************************
     * T11 - LOCATION COUNTERS 0-2, ABSOLUTE: CODE AND DATA ARE WRITTEN
     *       IN TURN AND EACH COUNTER CONTINUES WHERE IT LEFT OFF.
     ******************************************************************
               ORG  500
     START     CS   332
               CS
               MCW  MSG1,215
               W
               B    SUB
     BACK      MCW  MSG3,220
               W
               H    START
               LC   1
               ORG  1000
     MSG1      DCW  @HELLO@
     SUB       MCW  MSG2,220
               W
               B    MORE
               LC   2
               ORG  1500
     MSG3      DCW  @COUNTER 2@
               LC   0
     MORE      CS   332
               CS
               B    BACK
               LC   1
     MSG2      DCW  @WORLD@
               END  START
