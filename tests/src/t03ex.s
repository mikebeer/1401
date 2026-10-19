               JOB  T03 EX, LTORG, CHAIN, EQU, DSA, ORG, SFX
     ******************************************************************
     * T03 - A FIRST SEGMENT IS LOADED AND EXECUTED (EX), THEN THE
     *       SECOND SEGMENT IS LOADED OVER THE READ AREA BOOTSTRAP.
     *       FORWARD EQU, ORG TO A HUNDREDS BOUNDARY (X00), LTORG,
     *       CHAIN, DSA OF *, SFX, ACTUAL OP CODES IN CC 19-20.
     ******************************************************************
     LATER     EQU  EARLY+5
     EARLY     EQU  2000
               ORG  500
     SEG1      CS   332
               CS
               MCW  @SEGMENT 1@,209
               MCW  ADDR1,215
               MCW  ADDR2,220
               W
               R    40
               LTORG *
     ADDR1     DSA  LATER
     ADDR2     DSA  *
               EX   SEG1
               ORG  *&X00
     SEG2      CS   332
               CS
               MCW  @SEGMENT 2@,209
               MCW  LIT1,212
               B    SKIP
               SFX  A
     COUNT     DCW  000
               SFX
     SKIP      MCW  COUNTA,220
               A    @1@,COUNTA
               A    @1@,COUNTA
               MCW  COUNTA,225
               MCW  @ABCDEF@,240
               MCW  FIELD,FIELD2
               CHAIN 2
               MCW  FIELD2,250
                  2
               MCW  @DIRECT@,260
               W
               H    SEG2
     LIT1      DCW  @XYZ@
     FIELD     DCW  @123456@
     FIELD2    DCW  @......@
               ORG  1500
               LTORG *
               END  SEG2
