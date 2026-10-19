               JOB  T10 COLUMN BINARY
     ******************************************************************
     * T10 - COLUMN BINARY: READ A CARD (RCB), PRINT ITS BCD IMAGE
     *       AND THE 400 AND 500 AREAS, MOVE AND BINARY DECODE (MBD)
     *       TO A TAPE AREA, MOVE BINARY CODE (MBC) BACK, PUNCH IT
     *       (PCB), THEN READ THE NEXT CARD NORMALLY.
     ******************************************************************
               ORG  700
     TAPE      DS   160
     BREG      DCW  000
     START     CS   599
               CS
               CS   80
               RCB
               SBR  BREG
               CS   332
               CS
               MCW  @B AFTER RCB@,211
               MCW  BREG,215
               W
               CS   332
               CS
               SW   201
               MCW  80,280
               W
               SW   201
               MCW  480,280
               W
               SW   201
               MCW  580,280
               W
     *              MOVE AND BINARY DECODE: 401 501 402 502 ...
               SW   401
               MBD  580,TAPE
               SBR  BREG
               CS   332
               CS
               SW   201
               MCW  TAPE-80,280
               W
               SW   201
               MCW  TAPE,280
               W
               CS   332
               CS
               MCW  @B AFTER MBD@,211
               MCW  BREG,215
               W
     *              CLEAR THE CARD AREAS, MOVE BINARY CODE BACK
               CS   599
               CS
               SW   401
               MBC  TAPE,580
               SBR  BREG
               CS   332
               CS
               MCW  @B AFTER MBC@,211
               MCW  BREG,215
               W
               CS   332
               CS
               SW   201
               MCW  480,280
               W
               SW   201
               MCW  580,280
               W
               PCB
               SBR  BREG
               CS   332
               CS
               MCW  @B AFTER PCB@,211
               MCW  BREG,215
               W
               CS   332
               CS
               R
               SW   201
               MCW  80,280
               W
               H    START
               END  START
