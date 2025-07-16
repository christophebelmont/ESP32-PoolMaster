#include "Helpers.h"


// Removes the duplicates "/" from the string provided as an argument
void remove_duplicates_slash(char *str)
{
  int i,j,len,len1;

  /*calculating length*/
  for(len=0; str[len]!='\0'; len++);

  /*assign 0 to len1 - length of removed characters*/
  len1=0;

  /*Removing consecutive repeated characters from string*/
  for(i=0; i<(len-len1);)
  {
      if((str[i]==str[i+1]) && (str[i] == '/'))
      {
          /*shift all characters*/
          for(j=i;j<(len-len1);j++)
              str[j]=str[j+1];
          len1++;
      }
      else
      {
          i++;
      }
  }
}

// count number of items in a list
// the end of a list has a nullptr
uint8_t Helpers::count_items(const char * const * list) {
    uint8_t list_size = 0;
    if (list != nullptr) {
        while (list[list_size]) {
            list_size++;
        }
    }
    return list_size;
}

// count number of items in a list of lists
// the end of a list has a nullptr
uint8_t Helpers::count_items(const char * const ** list) {
    uint8_t list_size = 0;
    if (list != nullptr) {
        while (list[list_size]) {
            list_size++;
        }
    }
    return list_size;
}


// returns char pointer to translated description or fullname
// if force_en is true always take the EN non-translated word
const char * Helpers::translated_word(const char * const * strings, uint8_t language_index) {
    uint8_t index          = 0; // default en

    if (!strings) {
        return ""; // no translations
    }

    // see how many translations we have for this entity. if there is no translation for this, revert to EN
    if (Helpers::count_items(strings) >= language_index + 1 && strlen(strings[language_index])) {
        index = language_index;
    }

    return strings[index];
}


//Linear regression coefficients calculation function
// pass x and y arrays (pointers), lrCoef pointer, and n.
//The lrCoef array is comprised of the slope=lrCoef[0] and intercept=lrCoef[1].  n is the length of the x and y arrays.
//http://jwbrooks.blogspot.com/2014/02/arduino-linear-regression-function.html
void simpLinReg(float * x, float * y, double & lrCoef0, double & lrCoef1, int n)
{
  // initialize variables
  float xbar = 0;
  float ybar = 0;
  float xybar = 0;
  float xsqbar = 0;

  // calculations required for linear regression
  for (int i = 0; i < n; i++)
  {
    xbar += x[i];
    ybar += y[i];
    xybar += x[i] * y[i];
    xsqbar += x[i] * x[i];
  }

  xbar /= n;
  ybar /= n;
  xybar /= n;
  xsqbar /= n;

  // simple linear regression algorithm
  lrCoef0 = (xybar - xbar * ybar) / (xsqbar - xbar * xbar);
  lrCoef1 = ybar - lrCoef0 * xbar;
}
