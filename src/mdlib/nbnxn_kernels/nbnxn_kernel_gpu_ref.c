/* -*- mode: c; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup"; -*-
 *
 * 
 *                This source code is part of
 * 
 *                 G   R   O   M   A   C   S
 * 
 *          GROningen MAchine for Chemical Simulations
 * 
 *                        VERSION 3.2.0
 * Written by David van der Spoel, Erik Lindahl, Berk Hess, and others.
 * Copyright (c) 1991-2000, University of Groningen, The Netherlands.
 * Copyright (c) 2001-2004, The GROMACS development team,
 * check out http://www.gromacs.org for more information.

 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * If you want to redistribute modifications, please consider that
 * scientific software is very special. Version control is crucial -
 * bugs must be traceable. We will be happy to consider code for
 * inclusion in the official distribution, but derived work must not
 * be called official GROMACS. Details are found in the README & COPYING
 * files - if they are missing, get the official version at www.gromacs.org.
 * 
 * To help us fund GROMACS development, we humbly ask that you cite
 * the papers on the package - you can find them in the top README file.
 * 
 * For more info, check our website at http://www.gromacs.org
 * 
 * And Hey:
 * GROningen Mixture of Alchemy and Childrens' Stories
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>

#include "types/simple.h"
#include "vec.h"
#include "typedefs.h"
#include "nbnxn_kernel_gpu_ref.h"

void
nbnxn_kernel_gpu_ref(const nbnxn_pairlist_t     *nbl,
                     const nbnxn_atomdata_t     *nbat,
                     const interaction_const_t  *iconst,
                     real                       tabscale,  
                     const real *               VFtab,
                     gmx_bool                   clearF,
                     real *                     f,
                     real *                     fshift,
                     real *                     Vc,
                     real *                     Vvdw)
{
    const nbnxn_sci_t *nbln;
    const real    *x;
    real          rcut2;
    int           ntype,table_nelements,icoul,ivdw;
    real          facel,gbtabscale;
    int           n;
    int           ish3;
    int           sci;
    int           cj4_ind0,cj4_ind1,cj4_ind;
    int           ci,cj;
    int           ic,jc,ia,ja,is,js,im,jm;
    int           nnn,n0;
    int           ggid;
    real          shX,shY,shZ;
    real          fscal,tx,ty,tz;
    real          rinvsq;
    real          iq;
    real          qq,vcoul,krsq,vctot;
    int           nti,nvdwparam;
    int           tj;
    real          rt,r,eps,eps2,Y,F,Geps,Heps2,VV,FF,Fp,fijD,fijR;
    real          rinvsix;
    real          Vvdwtot;
    real          Vvdw_rep,Vvdw_disp;
    real          ix,iy,iz,fix,fiy,fiz;
    real          jx,jy,jz;
    real          dx,dy,dz,rsq,rinv;
    real          c6,c12,cexp1,cexp2,br;
    real *        charge;
    real *        shiftvec;
    real *        vdwparam;
    int *         shift;
    int *         type;
    const nbnxn_excl_t *excl[2];

    int           npair_tot,npair;
    int           nhwu,nhwu_pruned;

    if (clearF)
    {
        /* Zero the output force array */
        for(n=0; n<nbat->natoms*nbat->xstride; n++)
        {
            f[n] = 0;
        }
    }

    if (iconst->eeltype == eelCUT)
    {
        icoul           = 1;
    }
    else if (EEL_RF(iconst->eeltype))
    {
        icoul           = 2;
    }
    else 
    {
        icoul           = 3;
    }

    ivdw                = 1;

    /* avoid compiler warnings for cases that cannot happen */
    nnn                 = 0;
    vcoul               = 0.0;
    eps                 = 0.0;
    eps2                = 0.0;

    rcut2               = iconst->rvdw*iconst->rvdw;
	
    /* 3 VdW parameters for buckingham, otherwise 2 */
    nvdwparam           = (ivdw==2) ? 3 : 2;
    /* We currently pass the full Coulomb+LJ tables
    table_nelements     = (icoul==3) ? 4 : 0;
    table_nelements    += (ivdw==3) ? 8 : 0;
    */
    table_nelements     = 12;

    //charge              = mdatoms->chargeA;
    type                = nbat->type;
    facel               = iconst->epsfac;
    shiftvec            = nbat->shift_vec[0];
    vdwparam            = nbat->nbfp;
    ntype               = nbat->ntype;

    x = nbat->x;

    npair_tot   = 0;
    nhwu        = 0;
    nhwu_pruned = 0;

    for(n=0; n<nbl->nsci; n++)
    {
        nbln = &nbl->sci[n];

        ish3             = 3*nbln->shift;     
        shX              = shiftvec[ish3];  
        shY              = shiftvec[ish3+1];
        shZ              = shiftvec[ish3+2];
        cj4_ind0         = nbln->cj4_ind_start;      
        cj4_ind1         = nbln->cj4_ind_end;    
        sci              = nbln->sci;
        vctot            = 0;              
        Vvdwtot          = 0;              
        
        for(cj4_ind=cj4_ind0; (cj4_ind<cj4_ind1); cj4_ind++)
        {
            excl[0]           = &nbl->excl[nbl->cj4[cj4_ind].imei[0].excl_ind];
            excl[1]           = &nbl->excl[nbl->cj4[cj4_ind].imei[1].excl_ind];

            for(jm=0; jm<4; jm++)
            {
                cj               = nbl->cj4[cj4_ind].cj[jm];

                for(im=0; im<NSUBCELL; im++)
                {
                    /* We're only using the first imask,
                     * but here imei[1].imask is identical.
                     */
                    if ((nbl->cj4[cj4_ind].imei[0].imask >> (jm*NSUBCELL+im)) & 1)
                    {
                        ci               = sci*NSUBCELL + im;

                        npair = 0;
                        for(ic=0; ic<nbl->na_c; ic++)
                        {
                            ia               = ci*nbl->na_c + ic;
                    
                            is               = ia*nbat->xstride;
                            ix               = shX + x[is+0];
                            iy               = shY + x[is+1];
                            iz               = shZ + x[is+2];
                            iq               = facel*x[is+3];
                            nti              = nvdwparam*ntype*type[ia];
                    
                            fix              = 0;
                            fiy              = 0;
                            fiz              = 0;
                    
                            for(jc=0; jc<nbl->na_c; jc++)
                            {
                                ja               = cj*nbl->na_c + jc;
                        
                                if (!((excl[jc>>2]->pair[(jc & 3)*nbl->na_c+ic] >> (jm*NSUBCELL+im)) & 1))
                                {
                                    continue;
                                }

                                js               = ja*nbat->xstride;
                                jx               = x[js+0];      
                                jy               = x[js+1];      
                                jz               = x[js+2];      
                                dx               = ix - jx;      
                                dy               = iy - jy;      
                                dz               = iz - jz;      
                                rsq              = dx*dx+dy*dy+dz*dz;
                                if (rsq >= rcut2)
                                {
                                    continue;
                                }

                                if (type[ia] != ntype-1 && type[ja] != ntype-1)
                                {
                                    npair++;
                                }
                                rinv             = gmx_invsqrt(rsq);
                                rinvsq           = rinv*rinv;  
                                fscal            = 0;
                        
                                if(icoul==3 || ivdw==3)
                                {
                                    r                = rsq*rinv;
                                    rt               = r*tabscale;     
                                    n0               = rt;             
                                    eps              = rt-n0;          
                                    eps2             = eps*eps;        
                                    nnn              = table_nelements*n0;
                                }
                                
                                /* Coulomb interaction. icoul==0 means no interaction */
                                if (icoul > 0)
                                {
                                    qq               = iq*x[js+3];
                                    
                                    switch(icoul)
                                    {
                                    case 1:
                                        /* Vanilla cutoff coulomb */
                                        vcoul            = qq*rinv;      
                                        fscal            = vcoul*rinvsq; 
                                        break;
                                        
                                    case 2:
                                        /* Reaction-field */
                                        krsq             = iconst->k_rf*rsq;
                                        vcoul            = qq*(rinv+krsq-iconst->c_rf);
                                        fscal            = qq*(rinv-2.0*krsq)*rinvsq;
                                        break;
                                        
                                    case 3:
                                        /* Tabulated coulomb */
                                        Y                = VFtab[nnn];     
                                        F                = VFtab[nnn+1];   
                                        Geps             = eps*VFtab[nnn+2];
                                        Heps2            = eps2*VFtab[nnn+3];
                                        nnn             += 4;
                                        Fp               = F+Geps+Heps2;   
                                        VV               = Y+eps*Fp;       
                                        FF               = Fp+Geps+2.0*Heps2;
                                        vcoul            = qq*VV;          
                                        fscal            = -qq*FF*tabscale*rinv;
                                        break;
                                        
                                    case 4:
                                        /* GB */
                                        gmx_fatal(FARGS,"Death & horror! GB generic interaction not implemented.\n");
                                        break;
                                        
                                    default:
                                        gmx_fatal(FARGS,"Death & horror! No generic coulomb interaction for icoul=%d.\n",icoul);
                                        break;
                                    }
                                    vctot            = vctot+vcoul;    
                                } /* End of coulomb interactions */
                                
                                
                                /* VdW interaction. ivdw==0 means no interaction */
                                if(ivdw > 0)
                                {
                                    tj               = nti+nvdwparam*type[ja];
                                    
                                    switch(ivdw)
                                    {
                                    case 1:
                                        /* Vanilla Lennard-Jones cutoff */
                                        c6               = vdwparam[tj];   
                                        c12              = vdwparam[tj+1]; 
                                        
                                        rinvsix          = rinvsq*rinvsq*rinvsq;
                                        Vvdw_disp        = c6*rinvsix;     
                                        Vvdw_rep         = c12*rinvsix*rinvsix;
                                        fscal           += (Vvdw_rep-Vvdw_disp)*rinvsq;
                                        Vvdwtot          = Vvdwtot+Vvdw_rep/12-Vvdw_disp/6;
                                        break;
                                        
                                    case 2:
                                        /* Buckingham */
                                        c6               = vdwparam[tj];   
                                        cexp1            = vdwparam[tj+1]; 
                                        cexp2            = vdwparam[tj+2]; 
                                        
                                        rinvsix          = rinvsq*rinvsq*rinvsq;
                                        Vvdw_disp        = c6*rinvsix;     
                                        br               = cexp2*rsq*rinv;
                                        Vvdw_rep         = cexp1*exp(-br); 
                                        fscal           += (br*Vvdw_rep-6.0*Vvdw_disp)*rinvsq;
                                        Vvdwtot          = Vvdwtot+Vvdw_rep-Vvdw_disp;
                                        break;
                                        
                                    case 3:
                                        /* Tabulated VdW */
                                        c6               = vdwparam[tj];   
                                        c12              = vdwparam[tj+1]; 
                                        
                                        Y                = VFtab[nnn];     
                                        F                = VFtab[nnn+1];   
                                        Geps             = eps*VFtab[nnn+2];
                                        Heps2            = eps2*VFtab[nnn+3];
                                        Fp               = F+Geps+Heps2;   
                                        VV               = Y+eps*Fp;       
                                        FF               = Fp+Geps+2.0*Heps2;
                                        Vvdw_disp        = c6*VV;          
                                        fijD             = c6*FF;          
                                        nnn             += 4;          
                                        Y                = VFtab[nnn];     
                                        F                = VFtab[nnn+1];   
                                        Geps             = eps*VFtab[nnn+2];
                                        Heps2            = eps2*VFtab[nnn+3];
                                        Fp               = F+Geps+Heps2;   
                                        VV               = Y+eps*Fp;       
                                        FF               = Fp+Geps+2.0*Heps2;
                                        Vvdw_rep         = c12*VV;         
                                        fijR             = c12*FF;         
                                        fscal           += -(fijD+fijR)*tabscale*rinv;
                                        Vvdwtot          = Vvdwtot + Vvdw_disp + Vvdw_rep;
                                        break;
                                        
                                    default:
                                        gmx_fatal(FARGS,"Death & horror! No generic VdW interaction for ivdw=%d.\n",ivdw);
                                        break;
                                    }
                                } /* end VdW interactions */
                                
                                tx               = fscal*dx;
                                ty               = fscal*dy;
                                tz               = fscal*dz;
                                fix              = fix + tx;
                                fiy              = fiy + ty;
                                fiz              = fiz + tz;
                                f[js+0]         -= tx;
                                f[js+1]         -= ty;
                                f[js+2]         -= tz;
                            }
                    
                            f[is+0]          = f[is+0] + fix;
                            f[is+1]          = f[is+1] + fiy;
                            f[is+2]          = f[is+2] + fiz;
                            fshift[ish3]     = fshift[ish3]   + fix;
                            fshift[ish3+1]   = fshift[ish3+1] + fiy;
                            fshift[ish3+2]   = fshift[ish3+2] + fiz;

                            /* Count in half work-units.
                             * In CUDA one work-unit is 2 warps.
                             */
                            if ((ic+1) % (nbl->na_c/2) == 0)
                            {
                                npair_tot += npair;

                                nhwu++;
                                if (npair > 0)
                                {
                                    nhwu_pruned++;
                                }

                                npair = 0;
                            }
                        }
                    }
                }
            }
        }
        
        //ggid             = nlist->gid[n];         
        ggid = 0;
        Vc[ggid]         = Vc[ggid]   + vctot;
        Vvdw[ggid]       = Vvdw[ggid] + Vvdwtot;
    }

    if (debug)
    {
        fprintf(debug,"number of half %dx%d atom pairs: %d pruned: %d fraction %4.2f\n",
                nbl->na_c,nbl->na_c,
                nhwu,nhwu_pruned,nhwu_pruned/(double)nhwu);
        fprintf(debug,"generic kernel pair interactions:            %d\n",
                nhwu*nbl->na_c/2*nbl->na_c);
        fprintf(debug,"generic kernel post-prune pair interactions: %d\n",
                nhwu_pruned*nbl->na_c/2*nbl->na_c);
        fprintf(debug,"generic kernel non-zero pair interactions:   %d\n",
                npair_tot);
        fprintf(debug,"ratio non-zero/post-prune pair interactions: %4.2f\n",
                npair_tot/(double)(nhwu_pruned*nbl->na_c/2*nbl->na_c));
    }
}