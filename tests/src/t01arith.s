               JOB  T01 ARITHMETIC, COMPARE, EDIT
     ******************************************************************
     * T01 - ADD, SUBTRACT, ZERO ADD/SUBTRACT, OVERFLOW, MULTIPLY,
     *       DIVIDE, COMPARE (HIGH-LOW-EQUAL), MOVE AND EDIT, MCS.
     *       EACH RESULT IS PRINTED ON ONE LINE.
     ******************************************************************
               ORG  333
     START     CS   332
               CS
     * ADD AND SUBTRACT WITH SIGNS
               ZA   N123,W5
               A    N456,W5
               MCW  W5,205
               S    N1000,W5
               MCW  W5,212
               ZS   W5,W6
               MCW  W6,220
               A    N1000,W5
               MCW  W5,227
               ZA   NEG7,W5
               A    N3,W5
               MCW  W5,234
               S    NEG7,W5
               MCW  W5,241
               W
               CS   332
               CS
     * OVERFLOW
               ZA   N99999,W5
               A    N1,W5
               BAV  OVF1
               MCW  @NO OVERFLOW@,215
               B    NEXT1
     OVF1      MCW  @OVERFLOW@,215
     NEXT1     MCW  W5,225
               BAV  OVF2
               MCW  @CLEARED@,240
               B    NEXT2
     OVF2      MCW  @STILL ON@,240
     NEXT2     W
               CS   332
               CS
     * MULTIPLY 123 X 456 AND -1246 X 543
               ZA   N456,PROD-4
               M    N123,PROD
               MCW  PROD,207
               ZA   N543,PROD8-5
               M    NG1246,PROD8
               MCW  PROD8,218
               MCS  PROD8,230
               W
               CS   332
               CS
     * DIVIDE 1246 / 543 AND 1000000 / 7
               ZA   N1246,DIVW
               D    N543,DIVW-3
               MCW  DIVW,210
               ZA   MILL,DIVW2
               D    N7,DIVW2-6
               MCW  DIVW2,225
               W
               CS   332
               CS
     * COMPARE
               C    ABC,ABD
               BH   CH1
               MCW  @NOT HIGH@,210
               B    CN1
     CH1       MCW  @HIGH@,210
     CN1       C    ABD,ABC
               BL   CL1
               MCW  @NOT LOW@,220
               B    CN2
     CL1       MCW  @LOW@,220
     CN2       C    ABC,ABC2
               BE   CE1
               MCW  @NOT EQUAL@,230
               B    CN3
     CE1       MCW  @EQUAL@,230
     CN3       C    NUMS,LETS
               BH   CH2
               MCW  @DIGITS LOW@,245
               B    CN4
     CH2       MCW  @DIGITS HIGH@,245
     CN4       C    SHORT,LONG
               BU   CU1
               MCW  @SAME@,255
               B    CN5
     CU1       MCW  @UNEQUAL@,255
     CN5       W
               CS   332
               CS
     * EDIT
               LCA  ED1,215
               MCE  AMT1,215
               LCA  ED2,235
               MCE  AMT2,235
               LCA  ED3,255
               MCE  AMT3,255
               LCA  ED4,275
               MCE  AMT4,275
               LCA  ED5,295
               MCE  AMT5,295
               W
               CS   332
               CS
     * MOVE AND SUPPRESS ZEROS, MOVE ZONE, MOVE NUMERIC
               MCS  Z1,210
               MCS  Z2,220
               MCW  @ABC@,233
               MZ   @J@,233
               MN   @5@,232
               W
               H    START
     *
     W5        DCW  00000
     W6        DCW  000000
     N123      DCW  123
     N456      DCW  456
     N1000     DCW  1000
     N99999    DCW  99999
     N1        DCW  1
     N3        DCW  3
     N7        DCW  7
     NEG7      DCW  -7
     N543      DCW  543
     N1246     DCW  1246
     NG1246    DCW  -1246
     MILL      DCW  1000000
     PROD      DCW  0000000
     PROD8     DCW  00000000
     DIVW      DCW  00000000
     DIVW2     DCW  0000000000000
     ABC       DCW  @ABC@
     ABD       DCW  @ABD@
     ABC2      DCW  @ABC@
     NUMS      DCW  @123@
     LETS      DCW  @XYZ@
     SHORT     DCW  @AB@
     LONG      DCW  @XAB@
     ED1       DCW  @ ,  0.  @
     AMT1      DCW  0012345
     ED2       DCW  @$  ,  0.  &CR@
     AMT2      DCW  -0012345
     ED3       DCW  @ ,  0.  &CR@
     AMT3      DCW  0000007
     ED4       DCW  @*  ,  0.  @
     AMT4      DCW  0004567
     ED5       DCW  @$  ,   .  @
     AMT5      DCW  0000003
     Z1        DCW  0001230
     Z2        DCW  @0,0405@
               END  START
