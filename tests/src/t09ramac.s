               JOB  T09 1405 DISK STORAGE
     ******************************************************************
     * T09 - 1405 RAMAC: SEEK, WRITE AND READ A RECORD, WRITE CHECK,
     *       FULL TRACK, LOAD MODE, THE ADDRESS REGISTERS, AND THE
     *       INDICATORS V W X Y N (PRINTED AFTER EACH STEP).
     ******************************************************************
               ORG  400
     TXT       DCW  @ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCD@
     DASH      DCW  @----------------------------------------@
     RA        DCW  @00001230@
     GMA1      DCW  @}@
     AREA      DS   200
     GMA2      DCW  @}@
     RB        DCW  @00001200@
     GMB1      DCW  @}@
     TRK       DS   1000
     GMB2      DCW  @}@
     RC        DCW  @00001210@
     GMC1      DCW  @}@
     LAREA     DS   176
     GMC2      DCW  @}@
     RD        DCW  @00001230@
     GMD1      DCW  @}@
     SAREA     DS   50
     GMD2      DCW  @}@
     BREG      DCW  000
     START     CS   332
               CS
               MCW  TXT,AREA-160
               MCW  TXT,AREA-120
               MCW  TXT,AREA-80
               MCW  TXT,AREA-40
               MCW  TXT,AREA
               MCW  @SEEK 123@,208
               SD   RA-7
               B    SHOW
               MCW  @WRITE 123@,209
               WD   RA-7
               SBR  BREG
               B    SHOW
               MCW  @B AFTER WD@,210
               MCW  BREG,214
               W
               CS   332
               CS
               MCW  @WCHECK OK@,209
               WDC  RA-7
               B    SHOW
               MCW  @*@,AREA-100
               MCW  @WCHECK BAD@,210
               WDC  RA-7
               B    SHOW
               MCW  DASH,AREA-160
               MCW  DASH,AREA-120
               MCW  DASH,AREA-80
               MCW  DASH,AREA-40
               MCW  DASH,AREA
               MCW  @READ 123@,208
               RD   RA-7
               B    SHOW
               SW   201
               MCW  AREA-100,300
               W
               SW   201
               MCW  AREA,300
               W
               CS   332
               CS
     *              RECORD 130 IS ON ANOTHER TRACK: NO SEEK, NO READ
               MCW  @00001300@,RA
               MCW  @NOT SEEKED@,210
               RD   RA-7
               B    SHOW
     *              FULL TRACK: RECORDS 120-124, THEN READ FROM 122
               MCW  TXT,TRK-960
               MCW  TXT,TRK-760
               MCW  TXT,TRK-560
               MCW  TXT,TRK-360
               MCW  TXT,TRK-160
               MCW  @0@,TRK-999
               MCW  @1@,TRK-799
               MCW  @2@,TRK-599
               MCW  @3@,TRK-399
               MCW  @4@,TRK-199
               SD   RB-7
               MCW  @W TRACK@,207
               WDT  RB-7
               B    SHOW
               MCW  @00001220@,RB
               MCW  @R TRACK@,207
               RDT  RB-7
               SBR  BREG
               B    SHOW
               MCW  @B AFTER RDT@,211
               MCW  BREG,215
               W
               CS   332
               CS
               SW   201
               MCW  TRK-960,240
               W
               SW   201
               MCW  TRK-760,240
               W
               SW   201
               MCW  TRK-560,240
               W
               SW   201
               MCW  TRK-360,240
               W
               SW   201
               MCW  TRK-160,240
               W
               CS   332
               CS
     *              LOAD MODE: RECORD 121 WITH WORD MARKS
               MCW  TXT,LAREA-136
               SW   LAREA-175,LAREA-165
               SW   LAREA
               MCW  @LOAD WRITE@,210
               WDW  RC-7
               B    SHOW
               MCW  @LOAD CHECK@,210
               WDCW RC-7
               B    SHOW
               CW   LAREA-175,LAREA-165
               CW   LAREA
               MCW  @LOAD READ@,209
               RDW  RC-7
               SBR  BREG
               B    SHOW
               MCW  @B AFTER RDW@,211
               MCW  BREG,215
               W
               CS   332
               CS
               BW   WM1,LAREA-165
               B    NOWM1
     WM1       MCW  @WM AT 11@,208
               W
     NOWM1     BW   WM2,LAREA
               B    NOWM2
     WM2       MCW  @WM AT 176@,209
               W
               CS   332
               CS
     NOWM2     MCW  @00001210@,RA
               MCW  @MOVE READ@,209
               RD   RA-7
               B    SHOW
     *              WRONG LENGTH, NO SUCH ARM
               MCW  @SHORT@,205
               RD   RD-7
               B    SHOW
               MCW  @20001230@,RA
               MCW  @ARM 2@,205
               SD   RA-7
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
