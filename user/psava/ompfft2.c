/* 2-D FFT encapsulated (OpenMP version) */
/*
  Copyright (C) 2007 Colorado School of Mines
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <rsf.h>
/*^*/

#include "ompfft2.h"

#include "weutil.h"
/*^*/

/*------------------------------------------------------------*/
fft2d ompfft2_init(int n1_, 
		   int n2_,
		   int ompnth)
/*< initialize >*/
{
    int ompith;

    /*------------------------------------------------------------*/
    fft2d fft;
    fft = (fft2d) sf_alloc(1,sizeof(*fft));

    fft->n1 = n1_;
    fft->n2 = n2_;

    fft->ctrace = (kiss_fft_cpx**) sf_complexalloc2(fft->n2,ompnth);

    fft->forw1 = (kiss_fft_cfg*) sf_alloc(ompnth,sizeof(kiss_fft_cfg));
    fft->invs1 = (kiss_fft_cfg*) sf_alloc(ompnth,sizeof(kiss_fft_cfg));
    fft->forw2 = (kiss_fft_cfg*) sf_alloc(ompnth,sizeof(kiss_fft_cfg));
    fft->invs2 = (kiss_fft_cfg*) sf_alloc(ompnth,sizeof(kiss_fft_cfg));

    for(ompith=0; ompith<ompnth; ompith++) {
	fft->forw1[ompith] = kiss_fft_alloc(fft->n1,0,NULL,NULL);
	fft->invs1[ompith] = kiss_fft_alloc(fft->n1,1,NULL,NULL);
	fft->forw2[ompith] = kiss_fft_alloc(fft->n2,0,NULL,NULL);
	fft->invs2[ompith] = kiss_fft_alloc(fft->n2,1,NULL,NULL);

	if (NULL == fft->forw2[ompith] || NULL == fft->invs2[ompith] || 
	    NULL == fft->forw1[ompith] || NULL == fft->invs1[ompith]) 
	    sf_error("%s: KISS FFT allocation error",__FILE__);
    }

    fft->fftscale = 1./sqrtf(fft->n1*fft->n2);

    return fft;
}

/*------------------------------------------------------------*/
void ompfft2_close(fft2d fft)
/*< Free allocated storage >*/
{
    free(*fft->ctrace); free (fft->ctrace);

    free (*fft->forw2); free (fft->forw2);
    free (*fft->invs2); free (fft->invs2);
    free (*fft->forw1); free (fft->forw1);
    free (*fft->invs1); free (fft->invs1);
}

/*------------------------------------------------------------*/
void ompfft2(bool inv          /* inverse/forward flag */, 
	     kiss_fft_cpx **pp /* [1...n2][1...n1] */,
	     int ompith,
	     fft2d fft) 
/*< Apply 2-D FFT >*/
{
    int i1,i2;
    
    if (inv) {
	for (i2=0; i2 < fft->n2; i2++) {
#pragma omp critical
	    kiss_fft(fft->invs1[ompith],pp[i2],pp[i2]);
	}
	for (i1=0; i1 < fft->n1; i1++) {
#pragma omp critical
	    kiss_fft_stride(fft->invs2[ompith],pp[0]+i1,fft->ctrace[ompith],fft->n1);
	    for (i2=0; i2<fft->n2; i2++) {
		pp[i2][i1] = fft->ctrace[ompith][i2];
	    }
	}
	
	for     (i2=0; i2<fft->n2; i2++) {
	    for (i1=0; i1<fft->n1; i1++) {
		pp[i2][i1] = sf_crmul(pp[i2][i1],fft->fftscale);
	    }
	}
    } else {
	for     (i2=0; i2<fft->n2; i2++) {
	    for (i1=0; i1<fft->n1; i1++) {
		pp[i2][i1] = sf_crmul(pp[i2][i1],fft->fftscale);
	    }
	}
	
	for (i1=0; i1 < fft->n1; i1++) {
#pragma omp critical
	    kiss_fft_stride(fft->forw2[ompith],pp[0]+i1,fft->ctrace[ompith],fft->n1);
	    for (i2=0; i2<fft->n2; i2++) {
		pp[i2][i1] = fft->ctrace[ompith][i2];
	    }
	}
	for (i2=0; i2 < fft->n2; i2++) {
#pragma omp critical
	    kiss_fft(fft->forw1[ompith],pp[i2],pp[i2]);
	}
    }
}

/*------------------------------------------------------------*/
void ompsft2_init(float o1, 
		  float d1, 
		  float o2, 
		  float d2,
		  fft2d fft)
/*< origin shift >*/
{
    int i1,i2;
    float shift;

    fft->shf1 = sf_complexalloc(fft->n1);
    for( i1=0; i1<fft->n1; i1++) {
	shift = 2.0*SF_PI*i1/fft->n1*o1/d1;
	fft->shf1[i1] = sf_cmplx(cosf(shift),sinf(shift));
    }

    fft->shf2 = sf_complexalloc(fft->n2);
    for( i2=0; i2<fft->n2; i2++) {
	shift = 2.0*SF_PI*i2/fft->n2*o2/d2;
	fft->shf2[i2] = sf_cmplx(cosf(shift),sinf(shift));
    }
}

/*------------------------------------------------------------*/
void ompsft2_close(fft2d fft)
/*< close shift >*/
{
    free(fft->shf1);
    free(fft->shf2);
}

/*------------------------------------------------------------*/
void ompsft2(sf_complex **pp,
	     fft2d fft)
/*< apply shift >*/
{
    int i1,i2;

    for     (i2=0; i2<fft->n2; i2++) {
	for (i1=0; i1<fft->n1; i1++) {
#ifdef SF_HAS_COMPLEX_H
	    pp[i2][i1] *= fft->shf1[i1]*fft->shf2[i2];
#else
	    pp[i2][i1] = sf_cmul(pp[i2][i1],sf_cmul(fft->shf1[i1],fft->shf2[i2]));
#endif
	}
    }    
}

/*------------------------------------------------------------*/
void ompcnt2(sf_complex **pp,
	     fft2d fft)
/*< apply centering >*/
{
    int i1,i2;

    for(i2=1; i2<fft->n2; i2+=2) {
	for(i1=1; i1<fft->n1; i1+=2) {
#ifdef SF_HAS_COMPLEX_H
	    pp[i2][i1] = - pp[i2][i1];
#else
	    pp[i2][i1] = sf_cneg(pp[i2][i1]);
#endif
	}
    }
}
