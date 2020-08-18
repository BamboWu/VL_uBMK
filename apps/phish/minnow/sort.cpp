// MINNOW sort
// sort datums, emit top N

#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "phish/phish.h"

#include <functional>
#include <utility>
#include <string>
#include <vector>
#include <algorithm>

using namespace std;

void store(int);
void sort();

/* ---------------------------------------------------------------------- */

vector< pair<int,string> > list;
int ntop;

/* ---------------------------------------------------------------------- */

int main(int narg, char **args)
{
  phish_init(&narg,&args);
  phish_input(0,store,sort,1);
  phish_output(0);
  phish_check();

  if (narg != 2) phish_error("Sort syntax: sort N");
  ntop = atoi(args[1]);

  phish_loop();
  phish_exit();
}

/* ---------------------------------------------------------------------- */

void store(int nvalues)
{
  char *count,*word;
  int len;

  if (nvalues != 2) phish_error("Sort processes two-value datums");
  int type1 = phish_unpack(&count,&len);
  int type2 = phish_unpack(&word,&len);
  if (type1 != PHISH_INT32) phish_error("Sort processes int/string datums");
  if (type2 != PHISH_STRING) phish_error("Sort processes int/string datums");

  int icount = *(int32_t *) count;
  string str(word,strlen(word));
  list.push_back(make_pair(icount,str));
}

/* ---------------------------------------------------------------------- */

void sort()
{
  sort(list.begin(),list.end(),greater< pair<int,string> >());

  for (int i = 0; i < ntop; i++) {
    phish_pack_int32(list[i].first);
    phish_pack_string((char *) list[i].second.c_str());
    phish_send(0);
  }
}
