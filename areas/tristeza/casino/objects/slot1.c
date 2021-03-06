/******************************************************************
  Goblinoid slot machine 
  Coded by: Germ
  Last modified: Dec 11, 95

  Comments:  

  According to my calculations for this machine, the house margin
  is 3%.  If the major jackpot is excluded, the house margin is
  about 13%.  This is comparable to real slot machines.
  I spent a couple hours with my spreadsheet, designing this 
  machine.  It's not easy to get the odds to come out exactly 
  the way you want them. :)

  This machine is set up as follows:

   Symbol   Reel1  Reel2  Reel3
   ----------------------------
     orc      7      7      8
     warg     5      5      6
    troll     4      4      5 
     ogre     2      2      3
    giant     1      1      2
    titan     1      1      1
   ----------------------------
	     20     20     25

   This machine will "break" if any of two things happen:
     1) random() produces out-of range numbers
     2) the machine loses more than BROKEN_THRESHOLD

   Also, the evaluation function has been written so that
   no winning combination exists for all 3 random numbers equal.
   This prevents gold from being lost if random() gets "stuck"
   on one number.  I don't know how or why this might happen
   but hey, it's easy just to be safe.

   Things to do:
     - Code a progressive jackpot system, ie a jackpot that
       increases as people play the machine(s).

     - Allow players to insert varying amounts of coinage.

*******************************************************************/

#include <treasure.h>
#include <generic_items.h>

#include "../casino.h"

#define RECORD_WINS 1           /* 1=record them, 0=don't */ 
#define RECORD_LOSSES 1
#define SLOT_LOG "LOGS/SLOT1.LOG"
#define TP this_player()
#define QN query_name()
#define QRN query_real_name()
#define NAME capitalize(person->QN)
#define TRUE 1
#define FALSE 0
#define BET_AMOUNT 500
#define REELS 3                 /* How many reels on machine */
#define REEL1_SIZE 20           /* Total number of symbols per reel   */
#define REEL2_SIZE 20
#define REEL3_SIZE 25
#define BROKEN_THRESHOLD 2000000  /* Something is wrong if the  */
/* machine loses more than this. */

/*******************************************************************/
/*********************** function prototypes ***********************/
string spin_reel( int reelnum );
status payoff();
void spinning( string message );

/*******************************************************************/
/************************* global vars *****************************/

int machine_profits;    /* The cumulative profit since reset */
int spinning;           /* Is the wheel currently spinning?  */
int broken;             /* Did the wheel give away too much? */
int busy;               /* Has money been inserted?          */
int number_of_pulls;    /* Total pulls since reset           */
string *reel;
string name;            /* Who inserted the coins            */
object this_room;




/*******************************************************************
  Create the object and stuff
 *******************************************************************/

void create_treasure()
{
    int num;
    string desc;
    num = BET_AMOUNT;

    desc = "It is an old goblinoid slot machine standing against the wall.\
 On the front of the machine is a clear panel through which you can\
 see three reels with symbols etched on their outer surfaces.\
 Attached to the side of the machine is an arm with a handgrip, \
 and a plaque is affixed just below the panel. \
 You can \'insert coins\', \'pull arm\'.  This machine requires\
 insertion of "+num+" gold coins.\n"; 

    if ( (int)(TP->query_coder_level())  > 0 )
	desc += " Coders can \'view profits\'.\n";

    set_id( ({"slot","machine"}) );
    set_short("An goblinoid slot machine");
    set_long( desc );

    /*** initialize global vars ***/

    spinning = FALSE;
    broken = FALSE;
    busy = FALSE;
    machine_profits = 0;
    number_of_pulls = 0;

    reel = allocate( REELS );
}


status get() { return 0; }

void init_treasure()
{
    add_action("insert_cmd", "insert");
    add_action("pull_cmd", "pull");
    add_action("view_cmd", "view");
    add_action("read_cmd", "read"); 

    this_room = environment(TP);

}


/*******************************************************************
  Process the read command
*******************************************************************/

status read_cmd(string arg)
{
    string temp;

    if (!arg || sscanf(arg, "%s", temp ) != 1 || temp != "plaque" )
    {
	notify_fail("Read what?\n");
	return 0;
    }

    write("The payoffs for this machine are:\n\n\
              orc    orc         1000 coins\n\
             warg   warg         1500 coins\n\
       orc    orc    orc         1500 coins\n\
            troll  troll         2000 coins\n\
      warg   warg   warg         3000 coins\n\
             ogre   ogre         3500 coins\n\
     troll  troll  troll         4000 coins\n\
            giant  giant         5000 coins\n\
            titan  titan         7500 coins\n\
      ogre   ogre   ogre        12500 coins\n\
     giant  giant  giant        50000 coins\n\
     titan  titan  titan       500000 coins\n");

    return 1;

}


/*******************************************************************
  Process the insert command
*******************************************************************/

status insert_cmd(string arg)
{
    int howmuch;
    string temp;

    if (!arg || sscanf(arg, "%s", temp ) != 1 || temp != "coins" )
    {
	notify_fail("Insert what?\n"); 
	return 0;
    }

    if ( spinning == TRUE )
    {
	notify_fail("You must wait till the machine is ready.\n");
	return 0;
    }


    /* Broken can only happen if the wheel loses more than BROKEN_THRESHOLD */
    /* or if random() generates an incorrect number.                        */

    if ( broken == TRUE )
    {
	notify_fail("The machine is broken.  Please try again later.\n");
	return 0;
    }

    if ( busy == TRUE )
    {
	notify_fail("Someone has already inserted coins into this machine.\n");
	return 0;
    }


    howmuch = BET_AMOUNT;

    if (TP->query_money() < howmuch  )
    {
	notify_fail("You don\'t have "+howmuch+" coins to insert.\n");
	return 0;
    }

    TP->tell_me("Ok. You insert "+howmuch+" coins into the machine.");

    environment(TP)->tell_here(
      TP->QN + " inserts " +howmuch+ " coins into the machine.", TP);

    TP->add_money(-(howmuch));         /* subtract the bet from player's cash */

    busy = TRUE;

    return 1;
}


/***************************************************************************
  Process the pull command
 ***************************************************************************/

status pull_cmd(string arg)
{
    string temp; 
    int x;

    if (!arg || sscanf(arg, "%s", temp) != 1 || temp!="arm" )
    {
	notify_fail("Pull what?\n"); 
	return 0;
    }

    if ( spinning == TRUE )
    {
	notify_fail("You can\'t pull the arm while the machine is working.\n");
	return 0;
    }

    if ( busy == FALSE )
    {
	notify_fail("You must insert coins first.\n");
	return 0;
    }

    TP->tell_me("Ok. You pull the arm, and the reels spin!"); 
    this_room->tell_here(TP->QN + " pulls the arm, and the reels spin!", TP); 


    /* make sure that random() returns a number in the correct range */

    for ( x=0; x<REELS; x++ )
    {
	if ( (reel[x]=spin_reel(x)) == "error" )
	{
	    broken = TRUE;
	    notify_fail("The machine makes a grating noise.\n");
	    return 0;
	}
    }

    /*** everything seems ok...spin it! ***/

    name = TP->QRN;

    call_out( "spinning", 1, "The first reel stops on "+reel[0]+".");
    call_out( "spinning", 2, "The second reel stops on "+reel[1]+".");
    call_out( "payoff", 3 );

    number_of_pulls++;
    busy = FALSE;

    return 1;
}



/****************************************************************************
  Process 'view profits'
 ****************************************************************************/

status view_cmd(string arg)
{
    string temp;

    if (!arg || sscanf(arg, "%s", temp) != 1 || temp != "profits" )
    {
	notify_fail("View what?\n");
	return 0;
    }

    /*** only coders can check profits ***/

    if ( (int)(TP->query_coder_level()) > 0 ) 
    {
	TP->tell_me("The total profits for this machine are "+machine_profits+ 
	  " in " +number_of_pulls+ " spins.\n");
	return 1;
    }

    return 0;

}


/****************************************************************************
  Generate and return the winning symbol from the wheel
 ****************************************************************************/

string spin_reel( int reelnum )     
{
    int num;

    spinning = TRUE;

    /* Note..the values of num corresponding to the various monsters
       are arranged on each reel so that there are no winning combinations
       possible if all 3 random() functions produce the same number.
       This prevents the machine from giving away money if random()
       breaks and gets stuck on one number.  I don't know why it
       would do this, but hey why risk it.
    */

    if ( reelnum == 0 )
    {
	num = random( REEL1_SIZE );
	if (num<0)   return "error";
	if (num<7)   return "ORC";      /* 7 orcs on reel   */
	if (num<12)  return "WARG";     /* 5 wargs on reel  */
	if (num<16)  return "TROLL";    /* 4 trolls on reel */
	if (num<18)  return "OGRE";     /* 2 ogres on reel  */
	if (num==18) return "GIANT";    /* 1 giant on reel  */
	if (num==19) return "TITAN";    /* 1 titan on reel  */
    }
    else
    if ( reelnum == 1 )
    {
	num = random( REEL2_SIZE );
	if (num<0)   return "error";
	if (num<7)   return "ORC";      /* 7 orcs on reel   */
	if (num<12)  return "WARG";     /* 5 wargs on reel  */
	if (num<16)  return "TROLL";    /* 4 trolls on reel */
	if (num<18)  return "OGRE";     /* 2 ogres on reel  */
	if (num==18) return "GIANT";    /* 1 giants on reel */
	if (num==19) return "TITAN";    /* 1 titan on reel   */
    }
    else
    if ( reelnum == 2 )
    {
	num = random( REEL3_SIZE );
	if (num<0)   return "error";
	if (num==0)  return "TITAN";     /* 1 titan on reel  */    
	if (num<3)   return "GIANT";     /* 2 giant on reel  */
	if (num<6)   return "OGRE";      /* 3 ogres on reel  */
	if (num<11)  return "TROLL";     /* 5 trolls on reel */
	if (num<19)  return "ORC";       /* 8 orcs on reel   */
	if (num<25)  return "WARG";      /* 6 wargs on reel  */
    }

    return "error";
}


/****************************************************************************
  Evaluate the reels 
****************************************************************************/

int evaluate_reels()
{
    int match;

    if ( reel[1] != reel[2] )  return 0;      /** we lost **/

    match = ( reel[0] == reel[2] ) ? 3 : 2;

    if ( reel[1] == "ORC" )
    {
	if ( match == 2 )  return 2;           /** 2 orcs = 2:1 **/
	return 3;                              /** 3 orcs = 3:1 **/
    }

    if ( reel[1] == "WARG" )
    {
	if ( match == 2 )  return 3;           /** 2 wargs = 3:1 **/
	return 6;                              /** 3 wargs = 6:1 **/
    }

    if ( reel[1] == "TROLL" )
    {
	if ( match == 2 )  return 4;           /** 2 trolls = 4:1 **/
	return 8;                              /** 3 trolls = 8:1 **/
    } 

    if ( reel[1] == "OGRE" )
    {
	if ( match == 2 )  return 7;           /** 2 ogres =  7:1 **/
	return 25;                             /** 3 ogres = 25:1 **/
    }

    if ( reel[1] == "GIANT" )
    {
	if ( match == 2 )  return 10;          /** 2 giants =  10:1 **/
	return 100;                            /** 3 giants = 100:1 **/
    }

    if ( reel[1] == "TITAN" )
    {
	if ( match == 2 )  return 15;          /** 2 titans =   15:1 **/
	return 1000;                           /** 3 titans = 1000:1 **/
    }

    return 0;

} 


/****************************************************************************
  Payoff if we won 
****************************************************************************/

status payoff()
{  
    int result;
    object person, money;

    this_room->tell_here("The third reel stops on "+reel[2]+"!");

    result = evaluate_reels()*BET_AMOUNT;

    if ( result > 40000 )
	this_room->tell_here("JACKPOT!!!!!!");

    if ( person=present(name, this_room) )
    {
	if ( result > 0 )
	{                                        /* we won something */

	    person->tell_me("You win "+result+" gold coins!\n"); 

	    this_room->tell_here( person->QN
	      +" has won "+result+" gold coins.\n",person);

	    person->add_money(result);
	    machine_profits -= result - BET_AMOUNT; 

#if RECORD_WINS == TRUE
	    if ( !( person->query_coder_level()) )         /** lets not log coders **/
		write_file( AREA_PATH+SLOT_LOG, person->QRN+", "+result+", "
		  +reel[0]+" "+reel[1]+" "+reel[2]+", "
		  +machine_profits+", "+ number_of_pulls +"\n" );
#endif

	}
	else
	{                                        /* we did not win */
	    result = BET_AMOUNT;
	    machine_profits += result;

#if RECORD_LOSSES == TRUE
	    if ( !( person->query_coder_level()) )         /** lets not log coders **/
		write_file( AREA_PATH+SLOT_LOG, person->QRN+", "+result+", "
		  +reel[0]+" "+reel[1]+" "+reel[2]+", "
		  +machine_profits+", "+ number_of_pulls +"\n" );
#endif

	} 
    }
    else                                       /* winner isnt here */
    {
	if ( result > 0 )
	{
	    this_room->tell_here(result+" gold coins fall out of the machine!\n"); 
	    money = clone_object(GENERIC_MONEY_OBJECT);
	    money->set_money( result );
	    move_object( money, this_room );
	    machine_profits -= result - BET_AMOUNT;
	}
    }

    if ( machine_profits < -BROKEN_THRESHOLD )
    {
	broken = TRUE;
	this_room->tell_here("Suddenly you hear a grinding noise from the machine!\n");
    }


    name = ""; 
    spinning = FALSE;
    busy = FALSE;

    return 1;
}



/***************************************************************************
  Called by call_out to display the "reel spins" messages 
***************************************************************************/

void spinning( string message )
{
    this_room->tell_here( message );
}


/***************************************************************************/
/***************************************************************************/

