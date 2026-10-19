               JOB  T05 CARDS, PUNCH, CARRIAGE, LAST CARD
     ******************************************************************
     * T05 - READ DATA CARDS AFTER THE DECK UNTIL THE LAST CARD,
     *       PUNCH A COPY OF EACH, PRINT THEM WITH A SEQUENCE NUMBER,
     *       SKIP TO A NEW PAGE, PRINT WORD MARKS, SELECT STACKER.
     ******************************************************************
               ORG  400
     START     CS   332
               CS
               CS   180
               SW   1
               CC   1
     LOOP      R
               MCW  80,280
               MCW  80,180
               A    @1@,SEQ
               MCW  SEQ,205
               W
               P
               SS   4
               BLC  DONE
               B    LOOP
     DONE      CS   332
               CS
               MCW  @CARDS READ@,210
               MCW  SEQ,215
               CC   K
               W
               SW   205,210
               WM
               H    START
     SEQ       DCW  000
               END  START
