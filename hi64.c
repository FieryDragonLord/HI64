/******************************************************************************/
/* HI64 System Benchmark                                                      */
/* 64-bit benchmark derived from Hierarchical INTegration (HINT) originally   */
/* developed at Ames Laboratory, U.S. Department of Energy                    */
/*                                                                            */
/* Portions Copyright (C) 1994 Iowa State University Research Foundation, Inc.*/
/* Portions Copyright (C) 2003 Moritz Franosch                                */
/* Portions Copyright (C) 2014 Brian "DragonLord" Wong                        */
/*                                                                            */
/* This program is free software; you can redistribute it and/or modify       */
/* it under the terms of the GNU General Public License as published by       */
/* the Free Software Foundation.  You should have received a copy of the      */
/* GNU General Public License along with this program; if not, write to the   */
/* Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.    */
/*                                                                            */
/* Files needed for use:                                                      */
/*     * hi64.c            ---- Driver source                                 */
/*     * hkernel.c         ---- Kernel source                                 */
/*     * hi64.h            ---- General include file                          */
/*     * typedefs.h        ---- Include file for DSIZE and ISIZE              */
/*     * README.md         ---- Benchmark documentation and usage information */
/******************************************************************************/

/* Refer to hi64.h and typedefs.h for all-capitalized definitions.            */
#include       "hi64.h"    

int main(int argc, char *argv[])
{
    FILE    *curv;        /* Output file for QUIPS curve                      */
    char    filnm[160];   /* Output file name                                 */

    Speed   qdata[NSAMP]; /* Array to keep track of QUIPs and time            */

    ERROR   eflag;        /* Stopping condition returned from Hint            */

    ISIZE   imax,         /* Maximum representable index                      */
            itm,          /* Scratch values to find imax                      */
            n,            /* Number of goal subintervals                      */
            nscout;       /* Number of survey subintervals                    */

	volatile
	ISIZE   itm2;         /* Scratch to prevent use of registers              */
 
    volatile
    DSIZE   tm1;          /* Scratch to prevent use of registers              */

    DSIZE   dmax,         /* Maximum associative whole number                 */
            gamut,        /* Range of result from Hint, high minus low bound  */
            scx,          /* Scale factor for x values                        */
            scy,          /* Scale factor for y values                        */
            tm, tm2;      /* Scratch values to find dmax                      */

	double  memref,
			bandwt;

    double  delq,         /* Change in Quality                                */
			quips,        /* QUality Improvement Per Second                   */
            qpnet,        /* Net QUIPS; integral of QUIPS d(log t)            */
            qpeak,        /* Peak QUIPS attained                              */
            qprat,        /* Ratio of current QUIPS to peak QUIPS             */
            qptmp,        /* QUIPS temporary for calculating Net QUIPS        */
            t,            /* Time for a given trial                           */
            t0,           /* Starting time                                    */
            t1,           /* Ending time                                      */
            tdelta,       /* Timer resolution                                 */
         /* tlast, */     /* Time of last recorded trial                      */
            tscout;       /* Time for initial survey                          */

    int64_t dbits,        /* Number of bits of accuracy for dmax              */
            ibits,        /* Number of bits for imax                          */
            i, j, k,      /* Loop counters                                    */
            laps,         /* Approximate number of laps in a trial            */
            memuse,       /* Amount of memory used, in bytes                  */
            memuse2,      /* Same as memuse, but for keeping track of memory  */
                          /* usage during each loop for memory limiting       */
            memlimit;     /* Memory limit                                     */

    char*   suffix;       /* Suffix for data.suffix directory                 */

    printf("\nHI64 System Benchmark, Version 0.3.3");
    printf(" (January 29, 2015)\n");
    printf("Derived from HINT originally developed by");
    printf(" John L. Gustafson & Quinn O. Snell,\n");
    printf("Scalable Computing Laboratory, Iowa State University\n\n");
    printf("Portions Copyright (C) 1994");
    printf(" Iowa State University Research Foundation, Inc.\n");
    printf("Portions Copyright (C) 2003 Moritz Franosch\n");
    printf("Portions Copyright (C) 2014-2015 Brian \"DragonLord\" Wong\n\n");
    printf("This program is licensed under the GNU GPL; see COPYING.txt.\n");
    printf("NO WARRANTY OF ANY KIND IS PROVIDED, including any implied");
    printf(" warranty of\nmerchantability or fitness for a particular");
    printf(" purpose.\n\n");
    printf("See README.md for usage and technical information.\n");
    printf("---------------------------------------------------------\n");
	printf("RECT is %d bytes\n",sizeof(RECT));

if (argc >= 2) {
    memlimit = (int64_t)strtol(argv[1], NULL, 10) * 1048576;
}
else
    memlimit = 0x7fffffffffffffffLL;

if (memlimit <= 0)
    memlimit = 0x7fffffffffffffffLL;
if (memlimit < 0x7fffffffffffffffLL)
    printf("Memory use limited to %I64d MB\n", memlimit / 1048576 );

#ifdef DEBUG
    curv = stdout;
#else
    suffix="";
    if (argc>=3) {
      suffix=argv[2];
    }
    snprintf(filnm, 160, "data%s/%s", suffix, argv[0]);
    if ((curv = fopen(filnm, "w")) == NULL)
    {
        printf("Could not open data file\n");
        exit(1);
    }
#endif

 /* Attempt to find timer resolution. Loop until the time changes.            */
    for (t0 = When(); ((t1 = When()) == t0););
    tdelta = t1 - t0;

 /* Find the largest associative whole number, dmax.                          */
    dbits = 0;
    tm = (DSIZE)1;
    tm2 = tm + tm;
    tm1 = tm2 + 1;
 /* Double until it fails to increment or it overflows.                       */
    while (((tm1 - tm2) == (DSIZE)1) && (tm2 > (DSIZE)0)) 
    {
        dbits++;
        tm  += tm;
        tm2 += tm2;
        tm1  = tm2 + 1;
    }
    dbits++;
 /* We use a grid of dmax + 1 squares, but this might overflow, so back off 1.*/
    dmax = tm + (tm - 1);
    printf("Apparent number of bits of accuracy: %d\n", dbits);   
    printf("Maximum associative whole number:    %.0lf\n",(double)dmax);

 /* Find the largest representable index number.                              */
    ibits = 0;
    itm = (ISIZE)1;
    itm2 = itm + itm;
 /* Double it until it overflows.                                             */
    while (itm2 > (ISIZE)0) 
    {
        ibits++;
        itm  += itm;
        itm2 += itm2;
    }
    imax = itm;
    printf("Maximum number of bits of index:     %d\n", ibits);     
    printf("Maximum representable index:         %.0lf\n\n", (double)imax); 

 /* Calculate usable maxima, from whichever is most restrictive.              */
    if ((ibits + ibits) < dbits)
    {
        dmax = (DSIZE)imax * (DSIZE)imax - 1;
        dbits = ibits + ibits;
    }
    printf("Index-limited data accuracy:         %d bits\n", dbits); 
    printf("Maximum usable whole number:         %.0lf\n",(double)dmax);

 /* Half the bits, biased downward, go to x.                                  */
    j = (dbits)/2;         

 /* Now loop to take 2 to the appropriate power for grid dimensions.          */
    for (i = 0, scx = 1; i < j; scx += scx, i++);
    for (i = 0, scy = 1; i < dbits - j; scy += scy, i++);
    printf("Grid: %.0lf wide by %.0lf high.\n",(double)scx,(double)scy);


 /* This loop is used as a survey.                                            */
    for (nscout = NMIN, laps = 3; nscout < scx; )
    {
        t = Run(laps, &gamut, scx, scy, dmax, nscout, &eflag);

        if (eflag != NOERROR)
        {
            nscout /= 2;
            break;
        }
        else if ((t > RUNTM) && (eflag == NOERROR))
        {
            tscout = t;
            break;
        }
        else
        {
            tscout =  t;
            nscout *= 2;
            if (nscout > scx)
            {
                nscout /= 2;
                break;
            }
        }
    }
    if (tscout == 0)
    {
        printf( "Data type for %s is too small\n", argv[0]);
        exit(0);
    }
    if ((tscout < RUNTM) && (eflag == NOMEM))
       printf("Memory is not sufficient for > %3.1lf second runs.\n", RUNTM);
    else if (tscout < RUNTM)
       printf("Precision is not sufficient for > %3.1lf second runs.\n", RUNTM);


 /* This loop is the main loop driver of the HINT kernel.                     */
    for (t = 0, i = 0, n = NMIN, qpeak = 0, qprat = 1, memuse2 = 0; 
        ((i < NSAMP) && (t < STOPTM) && (n < scx) && (qprat > STOPRT)
            && (memuse2 * ADVANCE < memlimit));
        i++, n = ((int64_t)(n * ADVANCE) > n)? (n * ADVANCE) : n + 1)
    {     
        printf(".");
        fflush(stdout);

     /* Use enough laps to fill RUNTM time, roughly.                          */
        laps = MAX(RUNTM * nscout / (tscout * n), 1);
        t = Run(laps, &gamut, scx, scy, dmax, n, &eflag);
        if (t == 0)
            t = tdelta;

        if (eflag != NOERROR) {
	    printf("ERROR\n");
            break;
	}
            
     /* Calculate QUIPS. We must add 1 to dmax, but do it in steps.           */
     /* This is to avoid overflow of dmax                                     */
		delq = (double)dmax / gamut - 1;
        quips = delq / t + 1.0 / gamut / t;
        qdata[i].t  = t;
        qdata[i].qp = quips;
        qdata[i].delq = delq;
        qdata[i].n  = n;
        qpeak = MAX(qpeak, quips);
        qprat = quips / qpeak;
        memuse2 = (int64_t)(qdata[i].n * (sizeof(RECT)+sizeof(DSIZE)+sizeof(ISIZE)));
    }
    memuse = (int64_t)(qdata[i-1].n * (sizeof(RECT)+sizeof(DSIZE)+sizeof(ISIZE)));
    if ((qprat > STOPRT) && (eflag == NOMEM))
        printf("\nThis run was memory limited at %I64d subintervals -> %I64d bytes\n",
                                                 (int64_t)n, (int64_t)memuse);
    printf("\nDone with first pass. Now computing net QUIPS\n");

	memref = (DSREFS * sizeof(DSIZE) + ISREFS * sizeof(ISIZE)) * qdata[i-1].n;
	memref /= (1024 * 1024);
	bandwt = memref / qdata[i-1].t;
    fprintf(curv,"%12.10lf %lf %lf %I64d %20I64d %lf\n", 
            qdata[i-1].t, qdata[i-1].qp, qdata[i-1].delq,
            qdata[i-1].n, memuse, bandwt);
    
 /* Now go backwards through the data to calculate net QUIPS, and filter data.*/
    for (qpnet = qdata[i-1].qp, j = i - 2; j >= 0; j--)
    {
     /* If more work takes less time, we need to rerun the case of less work. */
        for (k = 0; ((qdata[j+1].t < qdata[j].t) && (k < PATIENCE)); k++)
        {
            printf("#"); /* todi */
            laps  = MAX(RUNTM * nscout / (tscout * qdata[j].n), 1);
            t = Run(laps, &gamut, scx, scy, dmax, qdata[j].n, &eflag);
            if (t == 0)
                t = tdelta;

	    delq = (double)dmax / gamut - 1;
            quips = delq / t + 1.0 / gamut / t;
            qdata[j].t  = t;
            qdata[j].qp = quips;
        }
        if (qdata[j+1].t < qdata[j].t)
        {
            printf(" Forcing a time for %d subintervals\n", qdata[j].n);
            qdata[j].t  = qdata[j+1].t;
			delq = (double)dmax / gamut - 1;
            qdata[j].qp = delq / qdata[j].t + 1.0 / gamut / qdata[j].t;
        }
        memuse = (int64_t)(qdata[j].n * (sizeof(RECT)+sizeof(DSIZE)+sizeof(ISIZE)));
		memref = (DSREFS * sizeof(DSIZE) + ISREFS * sizeof(ISIZE)) * qdata[j].n;
		memref /= (1024 * 1024);
		bandwt = memref / qdata[j].t;
        fprintf(curv,"%12.10lf %lf %lf %I64d %20I64d %lf\n", 
                qdata[j].t, qdata[j].qp, qdata[j].delq,
                qdata[j].n, memuse, bandwt);

     /* Now calculate the addition to the net QUIPS.                          */
     /* This is calculated as the sum of QUIPS(j) * (1 - time(j) / time(j+1)).*/
        qptmp = qdata[j].qp * qdata[j].t / qdata[j+1].t;
        qpnet += (qdata[j].qp - qptmp);
    }
    printf("\nFinished with %lf net QUIPs\n", qpnet);
    fclose(curv);
}



double
Run(int laps,
    DSIZE *gamut, DSIZE scx, DSIZE scy, DSIZE dmax, ISIZE memry, ERROR *eflag)
{
    RECT    *rect=NULL;   /* Array for saving hierarchical information        */
    ISIZE   *ixes=NULL;   /* Array for indices of queued entries              */
    DSIZE   *errs=NULL;   /* Array of error values                            */
 
    int     i, j;         /* Loop counters                                    */

    double  t0, t1, tm,   /* Time variables                                   */
            mint = 1e32;  /* Minimum time recorded for a given run            */

    *eflag = NOERROR;

 /* Allocate the memory for the arrays.                                       */
    rect = (RECT  *)malloc((MSIZE)(memry * sizeof(RECT)));
    errs = (DSIZE *)malloc((MSIZE)(memry * sizeof(DSIZE) * 2));
    ixes = (ISIZE *)malloc((MSIZE)(memry * sizeof(ISIZE) * 2));

 /* If the memory is unavailable, free what was allocated and return.         */
    if ((rect == NULL) || (errs == NULL) || (ixes == NULL))
    {
        if (rect != NULL)
            free(rect);
        if (errs != NULL)
            free(errs);
        if (ixes != NULL)
            free(ixes);
        *eflag = NOMEM;
        return (-NOMEM);
    }

    for (i = 0; i < NTRIAL; i++)
    {
        t0 = When();

     /* Run the benchmark for this trial.                                     */
        for (j = 0; j < laps; j++)
            *gamut = Hint(&scx, &scy, &dmax, &memry, rect, errs, ixes, eflag);

        t1 = When();

     /* Track the minimum time thus far for this trial.                       */
        tm = (t1 - t0) / laps;
        mint = MIN(mint, tm);
    }

 /* Free up the memory.                                                       */
    free(rect);
    free(errs);
    free(ixes);

    return (mint);
}
    
/* Return the current time in seconds, using a double precision number.       */
double
When()
{
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return ((double) tp.tv_sec + (double) tp.tv_usec * 1e-6);
}
