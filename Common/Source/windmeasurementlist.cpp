/***********************************************************************
**
**   windmeasurementlist.cpp
**
**   This file is part of Cumulus
**
************************************************************************
**
**   Copyright (c):  2002 by Andr� Somers
**
**   This file is distributed under the terms of the General Public
**   Licence. See the file COPYING for more information.
**
**   $Id$
**
***********************************************************************/

#include <math.h>
#include "windmeasurementlist.h"


WindMeasurementList::WindMeasurementList(NMEA_INFO *thenmeaInfo, DERIVED_INFO *thederivedInfo){
  nmeaInfo = thenmeaInfo;
  derivedInfo = thederivedInfo;
  nummeasurementlist = 0;
}


WindMeasurementList::~WindMeasurementList(){
  for (int i=0; i<nummeasurementlist; i++) {
    delete measurementlist[i];
  }
}


/**
  * Returns the weighted mean windvector over the stored values, or 0
  * if no valid vector could be calculated (for instance: too little or
  * too low quality data).
  */

extern int iround(double);

Vector WindMeasurementList::getWind(double alt, bool *found){
  //relative weight for each factor
  #define REL_FACTOR_QUALITY 100
  #define REL_FACTOR_ALTITUDE 100
  #define REL_FACTOR_TIME 200

  int altRange  = 1000; //conf->getWindAltitudeRange();
  int timeRange = 7200; //conf->getWindTimeRange();

  unsigned int total_quality=0;
  unsigned int quality=0, q_quality=0, a_quality=0, t_quality=0;
  Vector result;
  WindMeasurement * m;
  int now= (int)(nmeaInfo->Time);
  int altdiff=0;
  int timediff=0;

  *found = false;

  result.x = 0;
  result.y = 0;

  for(uint i=0;i< nummeasurementlist; i++) {
    m= measurementlist[i];
    altdiff= iround((alt - m->altitude));
    timediff= now - m->time;

    if (altdiff > -altRange && altdiff < altRange && timediff < timeRange) {

      q_quality = m->quality * REL_FACTOR_QUALITY / 5;
      //measurement quality

      a_quality = ((10*altRange) - (altdiff*altdiff/100))  * REL_FACTOR_ALTITUDE / (10*altRange);
      //factor in altitude difference between current altitude and
      //measurement.  Maximum alt difference is 1000 m.

      timediff=(timeRange-timediff)/10;
      t_quality = (((timediff)*(timediff))) * REL_FACTOR_TIME / (72*timeRange);
      //factor in timedifference. Maximum difference is 2 hours.

      quality= q_quality * a_quality * t_quality;

      result.x += m->vector.x * quality;
      result.y += m->vector.y * quality;
      total_quality+= quality;
    }
  }

  if (total_quality>0) {
    *found = true;
    result.x=result.x/(int)total_quality;
    result.y=result.y/(int)total_quality;
  }
  return result;
}


/** Adds the windvector vector with quality quality to the list. */
void WindMeasurementList::addMeasurement(Vector vector, double alt, int quality){
  uint index;
  if (nummeasurementlist==MAX_MEASUREMENTS) {
    index = getLeastImportantItem();
    delete measurementlist[index];
    nummeasurementlist--;
  } else {
    index = nummeasurementlist;
  }
  WindMeasurement * wind = new WindMeasurement;
  wind->vector.x = vector.x;
  wind->vector.y = vector.y;
  wind->quality=quality;
  wind->altitude=alt;
  wind->time= nmeaInfo->Time;
  measurementlist[index] = wind;
  nummeasurementlist++;
}

/**
 * getLeastImportantItem is called to identify the item that should be
 * removed if the list is too full. Reimplemented from LimitedList.
 */
uint WindMeasurementList::getLeastImportantItem() {

  int maxscore=0;
  int score=0;
  unsigned int founditem= nummeasurementlist-1;

  for (int i=founditem;i>=0;i--) {

    //Calculate the score of this item. The item with the highest score is the least important one.
    //We may need to adjust the proportion of the quality and the elapsed time. Currently, one
    //quality-point (scale: 1 to 5) is equal to 10 minutes.

    score=600*(6-measurementlist[i]->quality);
    score+= nmeaInfo->Time - measurementlist[i]->time;
    if (score>maxscore) {
      maxscore=score;
      founditem=i;
    }
  }
  return founditem;
}


/*
int WindMeasurementList::compareItems(QCollection::Item s1, QCollection::Item s2) {
  //return the difference between the altitudes in item 1 and item 2
  return (int)(((WindMeasurement*)s1)->altitude - ((WindMeasurement*)s2)->altitude).getMeters();
}
*/

