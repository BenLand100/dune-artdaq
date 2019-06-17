#include <stdio.h>

/* This assumes the first 10 channels of each SSP are wired up with channels and the last two
   are empty.  Assumes channels increase with Y */

int main(int argc, char **argv)
{
  int offlineAPAfromOnlineAPA[6] = {4,2,0,5,3,1};  // from Gina's APA list.
  int lastapa = -1;
  int modulecounter = 0;
  int phantommodulecounter = 0;  // nonexistent last two modules
  int cmc = 0;
  int phantomcmc = 0;

  for (int ionline=0;ionline<288;++ionline)
    {
      int APA = ionline / 48;
      int SSP = (ionline % 48) / 12;
      int Chan = ionline % 12;
      int SSPGlobal = ionline / 12;

      if (lastapa != APA)
	{
	  modulecounter = 0;
	  phantommodulecounter = 0;
	  lastapa = APA;
	  cmc = 0;
	  phantomcmc = 0;
	}

      int ioffline = ionline;  // very cheesy channel map
      int imodprint = 0;

      int oAPA = offlineAPAfromOnlineAPA[APA];
      if (Chan < 10) // use first ten chans of each SSP.  Is this right?
	{
	  ++cmc;
	  if (cmc == 5)
	    {
	      cmc = 1;
	      ++modulecounter;
	    }
	  int oapad2 = oAPA/2;
	  ioffline = 80*oapad2 + (cmc-1) + 8*modulecounter;
	  if (oAPA % 2) ioffline += 4;
	  imodprint = modulecounter;
	}
      else
	{
	  ++phantomcmc;
	  if (phantomcmc == 5)
	    {
	      phantomcmc = 1;
	      ++phantommodulecounter;
	    }
	  int oapad2 = oAPA/2;
	  ioffline = 16*oapad2 + (phantomcmc-1) + 8*phantommodulecounter + 240;
	  if (oAPA % 2) ioffline += 4;
	  imodprint = phantommodulecounter;
	}

      printf("%5d %5d %5d %5d %5d %5d %5d\n",ionline,APA,SSP,SSPGlobal,Chan,imodprint,ioffline);
    }
}
