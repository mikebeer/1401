               JOB  T04 MAGNETIC TAPE
     ******************************************************************
     * T04 - WRITE RECORDS ON TAPE 1 (MOVE AND LOAD MODE), A TAPE
     *       MARK, REWIND, READ THEM BACK, BACKSPACE, END OF FILE.
     ******************************************************************
               ORG  400
     REC       DCW  @RECORD 01 FOR TAPE@
     GM1       DCW  @}@
     WREC      DCW  @WM@
               DCW  @REC@
     GM2       DCW  @}@
     INBUF     DA   1X40,G
     START     CS   332
               CS
               SW   INBUF
               MCW  @01@,REC-9
               WT   1,REC-18
               MCW  @02@,REC-9
               WT   1,REC-18
               WTW  1,WREC-1
               WTM  1
               RWD  1
               MCW  @READ@,204
               W
     RLOOP     CS   332
               CS
               RT   1,INBUF
               BEF  EOF
               MCW  INBUF+19,220
               W
               B    RLOOP
     EOF       CS   332
               CS
               MCW  @END OF FILE@,211
               W
               BSP  1
               BSP  1
               RTW  1,INBUF
               MCW  INBUF+4,205
               W
               RWD  1
               H    START
               END  START
