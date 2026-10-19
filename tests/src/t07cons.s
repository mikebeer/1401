               JOB  T07 1407 CONSOLE INQUIRY STATION
     ******************************************************************
     * T07 - WRITE A MESSAGE ON THE 1407, READ AN ANSWER, PRINT IT.
     *       THE MESSAGE AND THE INPUT AREA END WITH A GROUP MARK WITH
     *       A WORD MARK; THE B-ADDRESS IS THEIR HIGH-ORDER POSITION.
     ******************************************************************
               ORG  400
     START     CS   332
               CS
               MCW  %T0,MSG-17,W
               MCW  %T0,INAREA,R
               MCW  INAREA+10,221
               MCW  @YOU SAID:@,209
               W
               LCA  MSG,250
               W
               H    START
     MSG       DCW  @WHAT IS YOUR NAME?@
     GM1       DCW  @}@
     INAREA    DA   1X20,G
               END  START
