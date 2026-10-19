               JOB  T02 AREA DEFINITION, INDEXING, LITERALS
     ******************************************************************
     * T02 - DA WITH SUBFIELDS, RECORD MARKS, GROUP MARK, CLEAR;
     *       INDEX REGISTERS; AREA-DEFINING AND ADDRESS LITERALS;
     *       MOVE RECORD (MCM); SAR/SBR; MODIFY ADDRESS (MA).
     ******************************************************************
               ORG  87
     X1        DSA  0
               ORG  92
     X2        DSA  0
               ORG  97
     X3        DSA  0
               ORG  400
     TABLE     DA   3X12,C
     NAME           1,8
     QTY            9,12
     RECS      DA   2X5,|,G
     RECA           1,5
     START     CS   332
               CS
               MCW  @ALPHA   0001@,TABLE+11
               MCW  @BRAVO   0020@,TABLE+23
               MCW  @CHARLIE 0300@,TABLE+35
               SBR  X1,0
               MCW  @003@,COUNT
     LOOP      MCW  NAME&X1,208
               MCW  QTY&X1,215
               A    QTY&X1,TOTAL
               W
               SBR  X1,12&X1
               S    @1@,COUNT
               C    COUNT,@000@
               BU   LOOP
               MCW  TOTAL,215
               MCW  @TOTAL@,205
               W
               CS   332
               CS
     * RECORD MARKS AND MOVE RECORD
               MCW  @HELLO@,RECS+4
               MCW  @WORLD@,RECS+10
               MCM  RECS,201
               MCM  RECS+6,210
               W
               CS   332
               CS
     * STORE ADDRESS REGISTERS AND MODIFY ADDRESS
               MCW  TOTAL,TOTAL2
               SAR  SAVA
               MCW  SAVA,205
               SBR  SAVB,TOTAL
               MCW  SAVB,210
               MCW  ADR1,ADRW
               MA   ADR2,ADRW
               MCW  ADRW,215
               MCW  +TABLE,220
               MCW  -TABLE,225
               MCW  +LONGL,230
               MCW  AREA1,240
               W
               H    START
     COUNT     DCW  000
     TOTAL     DCW  000000
     TOTAL2    DCW  000000
     SAVA      DCW  000
     SAVB      DCW  000
     ADR1      DSA  1234
     ADR2      DSA  3990
     ADRW      DCW  000
     LONGL     DCW  @A LONGER LITERAL@
               MCW  AREA1#7,AREA1
               END  START
