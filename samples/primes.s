               JOB  PRIMES BELOW 1000
     ******************************************************************
     * PRIMES - PRINT THE PRIME NUMBERS BELOW 1000, TEN TO A LINE.
     *   N RUNS THROUGH THE ODD NUMBERS 3, 5, ... 999.  EACH N IS
     *   DIVIDED BY 3, 5, 7, ... WHILE THE DIVISOR SQUARED IS NOT
     *   ABOVE N; A ZERO REMAINDER MEANS N IS NOT PRIME.
     *   SHOWS DIVIDE, MULTIPLY, COMPARE (C A,B SETS HIGH WHEN THE
     *   B-FIELD IS HIGHER), MOVE ZONE TO DROP SIGNS, MOVE AND
     *   SUPPRESS ZEROS, AND INDEX REGISTER 1 STEPPING THE PRINT LINE.
     ******************************************************************
               ORG  87
     X1        DSA  0
               ORG  333
     START     CS   332
               CS
               MCS  TWO,205
               SBR  X1,6
     NEXTN     A    @2@,N              N = N + 2
               C    LIMIT,N            N ABOVE 999?
               BH   DONE
               ZA   N,NN               N AS 7 DIGITS, UNSIGNED
               MZ   @0@,NN
               MCW  @003@,DIV
     TRYDIV    ZA   DIV,PROD-4         MULTIPLIER IN THE HIGH ORDER
               M    DIV,PROD           PROD = DIV * DIV
               MZ   @0@,PROD
               C    NN,PROD            DIV SQUARED ABOVE N?
               BH   PRIME
               ZA   N,WORK             DIVIDEND IN THE LOW ORDER
               D    DIV,WORK-3         B: HIGH ORDER OF THE DIVIDEND
               MCW  WORK,REM           THE REMAINDER (3 DIGITS)
               MZ   @0@,REM            WITHOUT ITS SIGN
               C    ZERO3,REM          (COMPARE NEEDS EQUAL LENGTHS)
               BE   NEXTN              DIVIDES: NOT PRIME
               A    @2@,DIV
               B    TRYDIV
     PRIME     MCS  N,205&X1
               A    @1@,COUNT
               SBR  X1,6&X1
               C    X1,LINEND
               BU   NEXTN
               W                       TEN ON THE LINE: PRINT IT
               CS   332
               CS
               SBR  X1,0
               B    NEXTN
     DONE      W
               CS   332
               CS
               MCW  @PRIMES BELOW 1000:@,218
               MCS  COUNT,223
               W
               H    START
     N         DCW  0001
     TWO       DCW  0002
     LIMIT     DCW  0999
     NN        DCW  0000000
     DIV       DCW  000
     PROD      DCW  0000000
     WORK      DCW  00000000
     REM       DCW  000
     ZERO3     DCW  000
     COUNT     DCW  001
     LINEND    DSA  60
               END  START
