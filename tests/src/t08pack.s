               JOB  T08 1311 DISK PACK
     ******************************************************************
     * T08 - 1311 DISK: SEEK, WRITE SECTORS, WRITE CHECK, READ, SECTOR
     *       COUNT, WRONG LENGTH, LOAD MODE WITH WORD MARKS, TRACK WITH
     *       ADDRESSES, UNEQUAL ADDRESS, DRIVE NOT READY.  THE
     *       INDICATORS V W X Y N ARE PRINTED AFTER EACH STEP.
     ******************************************************************
               ORG  400
     TXT       DCW  @ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCD@
     DASH      DCW  @----------------------------------------@
     DCF       DCW  @0001000002@
     AREA      DS   200
     GMA       DCW  @}@
     DCF2      DCW  @0001002001@
     AREA2     DS   50
     GMB       DCW  @}@
     DCF3      DCW  @0001010001@
     AREA3     DS   90
     GMC       DCW  @}@
     DCF4      DCW  @0001000002@
     AREA4     DS   212
     GMD       DCW  @}@
     DCF5      DCW  @0001020001@
     ADR5      DCW  @001029@
     AREA5     DS   100
     GME       DCW  @}@
     DCF6      DCW  @2000000001@
     AREA6     DS   100
     GMF       DCW  @}@
     START     CS   332
               CS
               MCW  TXT,AREA-160
               MCW  TXT,AREA-120
               MCW  TXT,AREA-80
               MCW  TXT,AREA-40
               MCW  TXT,AREA
               MCW  @SEEK@,204
               SD   0,DCF-9
               B    SHOW
               MCW  @WRITE 2@,207
               WD   1,DCF-9
               B    SHOW
               MCW  DCF,210
               W
               CS   332
               CS
               MCW  @0001000002@,DCF
               MCW  @WCHECK OK@,209
               WDC  3,DCF-9
               B    SHOW
               MCW  @*@,AREA-50
               MCW  @0001000002@,DCF
               MCW  @WCHECK BAD@,210
               WDC  3,DCF-9
               B    SHOW
     *              OVERWRITE THE AREA AND READ THE TWO SECTORS BACK
               MCW  DASH,AREA-160
               MCW  DASH,AREA-120
               MCW  DASH,AREA-80
               MCW  DASH,AREA-40
               MCW  DASH,AREA
               MCW  @0001000002@,DCF
               MCW  @READ 2@,206
               RD   1,DCF-9
               B    SHOW
               MCW  DCF,210
               W
               CS   332
               CS
               SW   201
               MCW  AREA-100,300
               W
               SW   201
               MCW  AREA,300
               W
               CS   332
               CS
     *              WRONG LENGTH: A 50 CHARACTER AREA
               MCW  @SHORT@,205
               RD   1,DCF2-9
               B    SHOW
               SW   201
               MCW  AREA2,250
               W
               CS   332
               CS
     *              LOAD MODE: 90 CHARACTERS WITH WORD MARKS
               MCW  TXT,AREA3-50
               MCW  TXT,AREA3-10
               SW   AREA3-89,AREA3-79
               SW   AREA3
               MCW  @LOAD WRITE@,210
               WDW  1,DCF3-9
               B    SHOW
               CW   AREA3-89,AREA3-79
               CW   AREA3
               MCW  @0001010001@,DCF3
               MCW  @LOAD READ@,209
               RDW  1,DCF3-9
               B    SHOW
               BW   WM1,AREA3-79
               B    NOWM1
     WM1       MCW  @WM AT 11@,208
               W
     NOWM1     BW   WM2,AREA3
               B    NOWM2
     WM2       MCW  @WM AT 90@,208
               W
     *              TRACK WITH ADDRESSES: SECTORS 1000 AND 1001
     NOWM2     MCW  @TRACK READ@,210
               RD   6,DCF4-9
               B    SHOW
               SW   201
               MCW  AREA4-106,306
               W
               SW   201
               MCW  AREA4,306
               W
               CS   332
               CS
     *              WRITE A SECTOR ADDRESS, THEN LOOK FOR THE OLD ONE
               MCW  @TRACK WRT@,209
               WD   6,DCF5-9
               B    SHOW
               MCW  @0001020001@,DCF5
               MCW  @UNEQUAL@,207
               RD   1,DCF5-9
               B    SHOW
               MCW  @0001020001@,DCF5
               MCW  @NEW ADDR@,208
               RD   6,DCF5-9
               B    SHOW
               MCW  ADR5,206
               W
     *              DRIVE 2 IS NOT MOUNTED
               MCW  @NO DRIVE@,208
               RD   1,DCF6-9
               B    SHOW
               H    START
     *              PRINT THE STEP NAME AND THE DISK INDICATORS
     SHOW      SBR  SHOWX+3
               MCW  @.....@,216
               BIN  SV,V
               B    S1
     SV        MCW  @V@,212
     S1        BIN  SW,W
               B    S2
     SW        MCW  @W@,213
     S2        BIN  SX,X
               B    S3
     SX        MCW  @X@,214
     S3        BIN  SY,Y
               B    S4
     SY        MCW  @Y@,215
     S4        BIN  SN,N
               B    S5
     SN        MCW  @N@,216
     S5        W
               CS   332
               CS
     SHOWX     B    000
               END  START
