/**********************************************************************
***********************************************************************
**                                                                   **
**  HU-Prolog         Public Domain Version       Release  2.029     **
**                                                                   **
**  Author(s): 87-89  Christian Horn, Mirko Dziadzka, Matthias Horn  **
**             90-95  Mirko Dziadzka                                 **
**                    Fachhoschule Furtwangen, Allgemeine Informatik **
**                    email: dziadzka@ai-lab.fh-furtwangen.de        **
**                                                                   **
**  HU-Prolog 2.029 can be copied, modified and distributed freely   **
**  for any non-commercial purposes under the conditions that all    **
**  files include this header and that the message                   **
**  "HU-Prolog public domain version" and the names of the authors   **
**  will be displayed, when HU-Prolog is called.                     **
**                                                                   **
***********************************************************************
**********************************************************************/


#include "config.h"
#include "types.h"
#include "atoms.h"
#include "extern.h"

#define INDEXING 1

GLOBAL ENV CHOICEPOINT = NIL_ENV;
GLOBAL TERM BCT = NIL_TERM;

GLOBAL boolean EXECUTE (CALLP, CALLENV)
    TERM CALLP;
    ENV CALLENV;
{
    ATOM A;
    ENV ENVP;
    CLAUSE CP;
    int ARITY;
    ENV CHP;
    ENV BASEENV;
    
    CALLP = mk2sons(name(CALLP),son(CALLP),NIL_ATOM,NIL_TERM);
    CHOICEPOINT = BASEENV = newenv();
    if(CALLENV == NIL_ENV)
        CALLENV = BASEENV;

CALLQ:
    /* Behandlung asyncron ausgeloester Interrupts */
    if(UserAbort)
    {
        UserAbort= false;
        if(clause(INTERRUPT_0) == NIL_CLAUSE) 
            ABORT(ABORTE);
        if(name(CALLP) != NIL_ATOM)
            CALLP=mk2sons(INTERRUPT_0,NIL_TERM,GOTOT,CALLP);
        else
            CALLP=mk2sons(INTERRUPT_0,NIL_TERM,NIL_ATOM,NIL_TERM);
    }

#if !NO_TRACE
    if (SPYTRACE && TRACE(CALL_0,CALLP,CALLENV)==false)
        goto FAILQ; 
#endif

    /* Sprungverteiler in Abhaengigkeit vom Typ des Aufrufes */
QUICKCALL:
    switch(class(A = name(CALLP)))
    {
    case NORMP:
        if((CP= clause(A)) != NIL_CLAUSE) 
            goto PROCQ; 
        if(A==FAIL_0) 
            goto  FAILQ;
        if(clause(UNKNOWN_1) != NIL_CLAUSE)
        {
            TERM T;
            T = mkfreevar();
            (void)UNIFY(1,CALLP,T,base(CALLENV),NIL_TERM,MAXDEPTH);
            if(name(br(CALLP)) != NIL_ATOM)
                CALLP=mk2sons(UNKNOWN_1,T,GOTOT,br(CALLP));
            else
                CALLP=mk2sons(UNKNOWN_1,T,NIL_ATOM,NIL_TERM);
            goto CALLQ;
        }
	START_WARNING();
	ws("WARNING: unknown relation ");
	wq(A);
	ws("/");
	ws(itoa(arity(A)));
	ws(" failed\n");
	END_WARNING();
        goto FAILQ; 

#if HIGHER_ORDER
    case VARCP:; /* sonderbehandlung von var_calls */
        if(var_call(A) && arity(A) > 1)
	{
	    TERM T;
	    /* check arg1(CALLP) */
	    T = son(CALLP);
	    deref_(T,base(CALLENV));
	    if(!norm_atom(name(T)) || arity(name(T)) != 0)
	    {
		ERRORFLAG=CALLE;
		goto ON_ERROR;
	    }
	    /* ok, build new term */
	    A = LOOKATOM(name(T), arity(A) - 1 , true);

	    T = mkfunc(A , br(son(CALLP)));

	    if(name(br(CALLP)) != NIL_ATOM)
		CALLP = mk2sons(A, T, GOTOT, br(CALLP));
	    else
		CALLP = mk2sons(A, T, NIL_ATOM, NIL_TERM);

	    goto CALLQ;
	}
	else
	{
	    ERRORFLAG=CALLE;
	    goto ON_ERROR;
	}
#endif
    case CUTP:
        /* Sonderbehandlung fuer das Praedikat !/0 */
        ENVP = CALLENV;
        {
            CP = rule(ENVP);
            while(is_older_env(BASEENV,ENVP)  && 
            (CP == IMPG || CP==ANDG || CP==OR1G 
                || CP==OR2G || CP==NIL_CLAUSE )
                )
            {
                ENVP= env(ENVP); 
                CP= rule(ENVP);
            }
        }
        CHOICEPOINT= choice(ENVP);
        goto RETURNQ; 

    case ARITHP:
        CALLP= DOEVAL(CALLP,CALLENV);
        if(ERRORFLAG != NOERROR)
            goto ON_ERROR;
        else
            goto RETURNQ; 

    case EVALP:
        if(CALLEVALPRED(CALLP,CALLENV))
            goto RETURNQ;
        else if(ERRORFLAG != NOERROR) 
            goto ON_ERROR;
        else
            goto FAILQ;

    case VARP:
        /* Behandlung von Variablen */
        {
            register TERM T;
            T= br(CALLP);
            deref_(CALLP,base(CALLENV));
            if(class(name(CALLP)) == VARP)
	    {
                /* ABORT(CALLE); */
		ERRORFLAG=CALLE;
		goto ON_ERROR;
	    }
            if (name(T) != NIL_ATOM)
                CALLP=mk2sons(name(CALLP),son(CALLP),GOTOT,T);
            else
                CALLP=mk2sons(name(CALLP),son(CALLP),NIL_ATOM,NIL_TERM);
        }
        goto QUICKCALL;

    case NUMBP:
	{
	    /* ABORT(CALLE); */
	    ERRORFLAG=CALLE;
	    goto ON_ERROR;
	}
        /*NOTREACHED*/

    case GOTOP:
        CALLP=son(CALLP);
        if (CALLP != NIL_TERM  && name(CALLP) != NIL_ATOM)
            goto CALLQ; 
        else
            goto RETURNQ; 


    case BTEVALP:
        BCT=NIL_TERM; 
REDOEVALQ:
        {
            /* Environment anlegen, neuen Choicepoint setzen */
            ENVP = is_older_env(CALLENV,CHOICEPOINT) ? 
                                CHOICEPOINT : CALLENV;
            rel_env_stack(ENVP);
            ENVP = newenv();

            choice(ENVP)        = CHP   =CHOICEPOINT;
            termtop(ENVP)       = mark_term_stack();
            trail(ENVP)         = mark_trail_stack();
            atomtop(ENVP)       = mark_atom_stack();
            stringtop(ENVP)     = mark_string_stack();
            call(ENVP)          = CALLP;
            env(ENVP)           = CALLENV;
            base(ENVP)          = NIL_TERM;
            rule(ENVP)          = NIL_CLAUSE;

            CHOICEPOINT = ENVP;

            /* Aufruf des Praedikates */
            if(CALLEVALPRED(CALLP,CALLENV))
            {
                bct(ENVP)=BCT;
                if (BCT == NIL_TERM)
                    CHOICEPOINT = CHP;
                goto RETURNQ;
            }
            else
                CHOICEPOINT = CHP;
            if(ERRORFLAG != NOERROR)
                goto ON_ERROR;
            else
                goto FAILQ;
        }
    default:
        /* Dieser Zweig darf nie erreicht werden */
        SYSTEMERROR("EXECUTE.1");
        /*NOTREACHED*/

    } /* switch */

ON_ERROR:
    {
        TERM T;
        if(clause(ERROR_2) == NIL_CLAUSE) 
            ABORT(ERRORFLAG);
        T = mk2sons(UNBOUNDT,NIL_TERM,INTT,ERRORFLAG);
        (void)UNIFY(1,CALLP,T,base(CALLENV),NIL_TERM,MAXDEPTH);
        if(name(br(CALLP)) != NIL_ATOM)
            CALLP=mk2sons(ERROR_2,T, GOTOT,br(CALLP));
        else
            CALLP=mk2sons(ERROR_2,T,NIL_ATOM,NIL_TERM);
        ERRORFLAG= NOERROR;
        goto CALLQ;
    }
PROCQ:
    /* CP ist naechste zu versuchende Klausel */
    /* A==name(CALLP) */

    /* Anlegen eines neuen Environments, dabei evt. Optimieren */
    ENVP = is_older_env(CALLENV,CHOICEPOINT) ? CHOICEPOINT : CALLENV;
    rel_env_stack(ENVP);
    ENVP = newenv();

    choice(ENVP)        = CHP   =CHOICEPOINT;
    termtop(ENVP)       = mark_term_stack();
    trail(ENVP)         = mark_trail_stack();
    atomtop(ENVP)       = mark_atom_stack();
    stringtop(ENVP)     = mark_string_stack();
    call(ENVP)          = CALLP;
    env(ENVP)           = CALLENV;


    /* Suchen einer Klausel, deren Kopf mit dem Call unifizierbar 
    ** ist, dabei wird evt. ueber den Hauptfunktor des ersten
    ** Arguments indexiert
    */
    if ((ARITY=arity(A))==0)
    {
        /* Parameterloser Aufruf, erste Klausel ist die gesuchte
        ** jetzt --> Platz fuer lokale Variablen anlegen und 
        ** initialisieren.
        ** evt. Choicepoint setzten
        */
        base(ENVP) = stackvar(nvars(CP));

        if (nextcl(CP) != NIL_CLAUSE)
            CHOICEPOINT= ENVP;
        goto UNIFIED;
    }

#if INDEXING
    /* Setze jetzt A auf den Namen des ersten Parameters */
    {
        register TERM T;
        T=son(CALLP);
        deref_(T,base(CALLENV));
        A=name(T);
    }
#endif
    for(;;)
    {
        CLAUSE CPP; /* wird naechste zu probierende Klausel */
        /* CP - erste zu probierende Klausel */

#if INDEXING
        if (func_atom(A) && !var_call(A))
        {
            register ATOM AA;
            /* Suche eine Klausel, deren erstes Argument
            ** unifizierbar sein koennte
            */
func:
            if (func_atom(AA=name(son(head(CP)))) && AA!=A &&
		!var_call(AA))
            {
		if ((CP=nextcl(CP)) != NIL_CLAUSE) goto func;
		goto FAILQ;
            }
            /* erstes Argument von CP hat den richtigen Typ
            ** jetzt nachsehen, ob eventuell noch weitere
            ** Klauseln ex. -> wenn ja CHOICEPOINT aendern
            ** CPP - naechste zu probierende Klausel
            */
            for (CPP=nextcl(CP);CPP != NIL_CLAUSE ;CPP=nextcl(CPP))
            {
                if (!func_atom(AA=name(son(head(CPP)))) || AA==A ||
		    var_call(AA))
                {
                    CHOICEPOINT= ENVP;
                    break;
                }
            }
            goto checkunify;
        }
        else
#endif
        if ((CPP=nextcl(CP)) != NIL_CLAUSE) /* test on CHOICEPOINT */
            CHOICEPOINT= ENVP;
checkunify:
            base(ENVP)  = stackvar(nvars(CP));
            if(UNIFY(ARITY,son(CALLP),son(head(CP)),
                base(CALLENV),base(ENVP),MAXDEPTH)
                ) goto UNIFIED;

        CHOICEPOINT = CHP; /* CHOICEPOINT Ruecksetzen */
        if (CPP == NIL_CLAUSE)    /* keine weitere Klausel */
            goto FAILQ;   
        CP = CPP;

        rel_term_stack(termtop(ENVP));

    }

UNIFIED:

    /* Unify war erfolgreich, jetzt noch das environment 
    ** vervollstaendigen, envtop akt.
    */

    rule(ENVP)=CP;

    /* und den Klauselkoerper abarbeiten */
    {
        register TERM T;
        if (name(T= body(CP)) != NIL_ATOM)
        {
            CALLENV= ENVP; 
            CALLP=T; 
            goto CALLQ; 
        }
    }


RETURNQ:
    /* The subgoal in CALLP has just succeeded. */

#if !NO_TRACE
    if(SPYTRACE) (void)TRACE(PROVED_0,CALLP,CALLENV);
#endif
    if(is_older_env(BASEENV,CALLENV)) /*Es gibt noch einen Vorgeanger*/
    {
        if(CALLP != NIL_TERM  && name(next_br(CALLP)) != NIL_ATOM)
            goto  CALLQ;
        CALLP= call(CALLENV);
        CALLENV=env(CALLENV);
        goto RETURNQ;  
    }
    return true; 

FAILQ:
    /* Irgendetwas ist fehl geschlagen. 
    ** Weitermachen bei CHOICEPOINT 
    */
#if !NO_TRACE
    if(SPYTRACE) 
        (void)TRACE(FAILED_0,CALLP,CALLENV);
#endif
    if(!is_older_env(BASEENV,CHOICEPOINT)) 
        return false;  

    CALLP= call(CHOICEPOINT);
    CALLENV= env(CHOICEPOINT);
    CP= rule(CHOICEPOINT);
    rel_atom_stack(atomtop(CHOICEPOINT));
    rel_string_stack(stringtop(CHOICEPOINT));
    rel_term_stack(termtop(CHOICEPOINT));
    rel_trail_stack(trail(CHOICEPOINT));
    BCT = bct(CHOICEPOINT);

    CHOICEPOINT= choice(CHOICEPOINT);

    if (class(A=name(CALLP)) == BTEVALP)
    {
        if(BCT == NIL_TERM) 
            goto FAILQ;
#if !NO_TRACE
        if (SPYTRACE && TRACE(REDO_0,CALLP,CALLENV) == false) 
                goto  FAILQ;
#endif
        goto REDOEVALQ; 
    }

    if( CP == DUMMYCL) 
        CP = clause(A);
    else if (CP == NIL_CLAUSE) 
        goto FAILQ; 
    else 
        CP= nextcl(CP);

    if (CP==NIL_CLAUSE) 
        goto  FAILQ;
#if !NO_TRACE
    if(SPYTRACE && TRACE(REDO_0,CALLP,CALLENV) == false)
            goto  FAILQ;
#endif
    goto PROCQ;
}


/* end of file */
