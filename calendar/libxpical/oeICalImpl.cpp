/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is OEone Calendar Code, released October 31st, 2001.
 *
 * The Initial Developer of the Original Code is
 * OEone Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Mostafa Hosseini <mostafah@oeone.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef WIN32
#include <unistd.h>
#endif

#define __cplusplus__ 1

#include <vector>
#include "oeICalImpl.h"
#include "oeICalEventImpl.h"
#include "nsMemory.h"
#include "stdlib.h"   
#include "nsCOMPtr.h"
#include "nsISimpleEnumerator.h"
#include "nsString.h"
#include "nsIURL.h"
#include "nsNetCID.h"
#include "nsEscape.h"
#include "nsISupportsArray.h"
                             
extern "C" {
    #include "icalss.h"
}

icaltimetype ConvertFromPrtime( PRTime indate );
PRTime ConvertToPrtime ( icaltimetype indate );
void AlarmTimerCallback(nsITimer *aTimer, void *aClosure);

icalcomponent* icalcomponent_fetch( icalcomponent* parent,const char* uid )
{
    icalcomponent *inner;
	icalproperty *prop;
    const char *this_uid;

	for(inner = icalcomponent_get_first_component( parent , ICAL_ANY_COMPONENT );
		inner != 0;
		inner = icalcomponent_get_next_component( parent , ICAL_ANY_COMPONENT ) ){

		prop = icalcomponent_get_first_property(inner,ICAL_UID_PROPERTY);
    	if ( prop ) {
			this_uid = icalproperty_get_uid( prop );

			if(this_uid==0){
				icalerror_warn("icalcomponent_fetch found a component with no UID");
				continue;
			}
			if (strcmp(uid,this_uid)==0){
				return inner;
			}
		} else {
            icalerror_warn("icalcomponent_fetch found a component with no UID");
        }
    }
    return nsnull;
}

/* event enumerator */
class
oeEventEnumerator : public nsISimpleEnumerator
{
  public:
    oeEventEnumerator();
    virtual ~oeEventEnumerator();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator interface
    NS_DECL_NSISIMPLEENUMERATOR

    NS_IMETHOD AddEvent( oeIICalEvent *event );

  private:
    PRUint32 mCurrentIndex;
    std::vector<oeIICalEvent *> mEventVector;
};

oeEventEnumerator::oeEventEnumerator( ) 
:
    mCurrentIndex( 0 )
{
    NS_INIT_REFCNT();
}

oeEventEnumerator::~oeEventEnumerator()
{
    mEventVector.clear();
}

NS_IMPL_ISUPPORTS1(oeEventEnumerator, nsISimpleEnumerator)

NS_IMETHODIMP
oeEventEnumerator::HasMoreElements(PRBool *result)
{
    if( mCurrentIndex >= mEventVector.size() )
    {
        *result = PR_FALSE;
    }
    else
    {
        *result = PR_TRUE;
    }
    return NS_OK;
}
/*
NS_IMETHODIMP
oeEventEnumerator::GetNext(nsISupports **_retval)
{
        
    if( mCurrentIndex >= mEventVector.size() )
    {
        *_retval = nsnull;
    }
    else
    {
        oeIICalEvent* event = mEventVector[ mCurrentIndex ];
        event->AddRef();
        *_retval = event;
        ++mCurrentIndex;
    }
    
    return NS_OK;
}*/
NS_IMETHODIMP
oeEventEnumerator::GetNext(nsISupports **_retval)
{
        
    if( mCurrentIndex >= mEventVector.size() )
    {
        *_retval = nsnull;
    }
    else
    {
        oeIICalEvent* event = mEventVector[ mCurrentIndex ];
        nsresult rv = NS_NewICalEventDisplay( event, (oeIICalEventDisplay**) _retval );
        ++mCurrentIndex;
        return rv;
    }
    
    return NS_OK;
}


NS_IMETHODIMP
oeEventEnumerator::AddEvent(oeIICalEvent* event)
{
    mEventVector.push_back( event );
    return NS_OK;
}

/* date enumerator */
oeDateEnumerator::oeDateEnumerator( ) 
:
    mCurrentIndex( 0 )
{
    NS_INIT_REFCNT();
}

oeDateEnumerator::~oeDateEnumerator()
{
    mDateVector.clear();
}

NS_IMPL_ISUPPORTS1(oeDateEnumerator, nsISimpleEnumerator)

NS_IMETHODIMP
oeDateEnumerator::HasMoreElements(PRBool *result)
{
    if( mCurrentIndex >= mDateVector.size() )
    {
        *result = PR_FALSE;
    }
    else
    {
        *result = PR_TRUE;
    }
    return NS_OK;
}

NS_IMETHODIMP 
oeDateEnumerator::GetNext(nsISupports **_retval) 
{ 

    if( mCurrentIndex >= mDateVector.size() ) 
    { 
        *_retval = nsnull; 
    } 
    else 
    { 
        nsresult rv; 
        nsCOMPtr<nsISupportsPRTime> nsPRTime = do_CreateInstance( NS_SUPPORTS_PRTIME_CONTRACTID , &rv); 
        if( NS_FAILED( rv ) ) { 
            *_retval = nsnull; 
            return rv; 
        } 
        nsISupportsPRTime* date; 
        rv = nsPRTime->QueryInterface(NS_GET_IID(nsISupportsPRTime), (void **)&date); 
        if( NS_FAILED( rv ) ) { 
            *_retval = nsnull; 
            return rv; 
        } 

        date->SetData( mDateVector[ mCurrentIndex ] ); 
        *_retval = date; 
        ++mCurrentIndex; 
    } 

    return NS_OK; 
} 

NS_IMETHODIMP
oeDateEnumerator::AddDate(PRTime date)
{
    mDateVector.push_back( date );
    return NS_OK;
}







//////////////////////////////////////////////////
//   ICal Factory
//////////////////////////////////////////////////

nsresult
NS_NewICal( oeIICal** inst )
{
    NS_PRECONDITION(inst != nsnull, "null ptr");
    if (! inst)
        return NS_ERROR_NULL_POINTER;

    *inst = new oeICalImpl();
    if (! *inst)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*inst);
    return NS_OK;
}

oeICalImpl::oeICalImpl()
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::oeICalImpl()\n" );
#endif
        NS_INIT_REFCNT();

        m_batchMode = false;

        m_alarmtimer = nsnull;
}

oeICalImpl::~oeICalImpl()
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::~oeICalImpl()\n" );
#endif
    for( unsigned int i=0; i<m_observerlist.size(); i++ ) {
        m_observerlist[i]->Release();
    }
    m_observerlist.clear();
    if( m_alarmtimer  ) {
        if ( m_alarmtimer->GetDelay() != 0 )
            m_alarmtimer->Cancel();
        m_alarmtimer->Release();
        m_alarmtimer = nsnull;
    }
}

/**
 * NS_IMPL_ISUPPORTS1 expands to a simple implementation of the nsISupports
 * interface.  This includes a proper implementation of AddRef, Release,
 * and QueryInterface.  If this class supported more interfaces than just
 * nsISupports, 
 * you could use NS_IMPL_ADDREF() and NS_IMPL_RELEASE() to take care of the
 * simple stuff, but you would have to create QueryInterface on your own.
 * nsSampleFactory.cpp is an example of this approach.
 * Notice that the second parameter to the macro is the static IID accessor
 * method, and NOT the #defined IID.
 */
NS_IMPL_ISUPPORTS1(oeICalImpl, oeIICal);

/**
*
*   Test
*
*   DESCRIPTION: An exported XPCOM function used by the tester program to make sure
*		 all main internal functions are working properly
*   PARAMETERS: 
*	None
*
*   RETURN
*      None
*/
NS_IMETHODIMP
oeICalImpl::Test(void)
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::Test()\n" );
#endif
    icalfileset *stream;
    icalcomponent *icalcalendar,*icalevent;
    struct icaltimetype start, end;
    char uidstr[10]="900000000";
    char icalrawcalendarstr[] = "BEGIN:VCALENDAR\n\
BEGIN:VEVENT\n\
UID:900000000\n\
END:VEVENT\n\
END:VCALENDAR\n\
";
    
    stream = icalfileset_new(serveraddr);
    if ( !stream ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: Cannot open stream: %s!\n", serveraddr );
        #endif
        return NS_OK;
    }

    icalcalendar = icalparser_parse_string(icalrawcalendarstr);
    if ( !icalcalendar ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: Cannot create VCALENDAR!\n" );
        #endif
        return NS_OK;
    }

    icalevent = icalcomponent_get_first_component(icalcalendar,ICAL_VEVENT_COMPONENT);
    if ( !icalevent ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: VEVENT not found!\n" );
        #endif
        return NS_OK;
    }

    icalproperty *uid = icalproperty_new_uid( uidstr );
    icalcomponent_add_property( icalevent, uid );

    icalproperty *title = icalproperty_new_summary( "Lunch time" );
    icalcomponent_add_property( icalevent, title );
    
    icalproperty *description = icalproperty_new_description( "Will be out for one hour" );
    icalcomponent_add_property( icalevent, description );
    
    icalproperty *location = icalproperty_new_location( "Restaurant" );
    icalcomponent_add_property( icalevent, location );
    
    icalproperty *category = icalproperty_new_categories( "Personal" );
    icalcomponent_add_property( icalevent, category );

    icalproperty *classp = icalproperty_new_class( ICAL_CLASS_PRIVATE );
    icalcomponent_add_property( icalevent, classp );

    start.year = 2001; start.month = 8; start.day = 15;
    start.hour = 12; start.minute = 24; start.second = 0;
    start.is_utc = false; start.is_date = false;

    end.year = 2001; end.month = 8; end.day = 15;
    end.hour = 13; end.minute = 24; end.second = 0;
    end.is_utc = false; end.is_date = false;

    icalproperty *dtstart = icalproperty_new_dtstart( start );
    icalproperty *dtend = icalproperty_new_dtend( end );
    
    icalcomponent_add_property( icalevent, dtstart );
    icalcomponent_add_property( icalevent, dtend );
    //
    icalproperty *xprop = icalproperty_new_x( "TRUE" );
    icalparameter *xpar = icalparameter_new_member( "AllDay" ); //NOTE: new_member is used because new_x didn't work
                                                                //     Better to be changed to new_x when problem is solved
    icalproperty_add_parameter( xprop, xpar );
    
    icalcomponent_add_property( icalevent, xprop );
    //
    xprop = icalproperty_new_x( "FALSE" );
    xpar = icalparameter_new_member( "Alarm" );

    icalproperty_add_parameter( xprop, xpar );

    icalcomponent_add_property( icalevent, xprop );
    //
    xprop = icalproperty_new_x( "FALSE" );
    xpar = icalparameter_new_member( "AlarmWentOff" );

    icalproperty_add_parameter( xprop, xpar );

    icalcomponent_add_property( icalevent, xprop );
    //
    xprop = icalproperty_new_x( "5" );
    xpar = icalparameter_new_member( "AlarmLength" );

    icalproperty_add_parameter( xprop, xpar );

    icalcomponent_add_property( icalevent, xprop );
    //
    xprop = icalproperty_new_x( "mostafah@oeone.com" );
    xpar = icalparameter_new_member( "AlarmEmailAddres" );

    icalproperty_add_parameter( xprop, xpar );

    icalcomponent_add_property( icalevent, xprop );
    //
    xprop = icalproperty_new_x( "mostafah@oeone.com" );
    xpar = icalparameter_new_member( "InviteEmailAddres" );

    icalproperty_add_parameter( xprop, xpar );

    icalcomponent_add_property( icalevent, xprop );
    //
    xprop = icalproperty_new_x( "5" );
    xpar = icalparameter_new_member( "SnoozeTime" );

    icalproperty_add_parameter( xprop, xpar );

    icalcomponent_add_property( icalevent, xprop );
    //
    xprop = icalproperty_new_x( "0" );
    xpar = icalparameter_new_member( "RecurType" );

    icalproperty_add_parameter( xprop, xpar );

    icalcomponent_add_property( icalevent, xprop );
    //
    xprop = icalproperty_new_x( "0" );
    xpar = icalparameter_new_member( "RecurInterval" );

    icalproperty_add_parameter( xprop, xpar );

    icalcomponent_add_property( icalevent, xprop );
    //
    xprop = icalproperty_new_x( "days" );
    xpar = icalparameter_new_member( "RepeatUnits" );

    icalproperty_add_parameter( xprop, xpar );

    icalcomponent_add_property( icalevent, xprop );
    //
    xprop = icalproperty_new_x( "FALSE" );
    xpar = icalparameter_new_member( "RepeatForever" );

    icalproperty_add_parameter( xprop, xpar );

    icalcomponent_add_property( icalevent, xprop );
    //

    struct icaltriggertype trig;
    icalcomponent *valarm = icalcomponent_new_valarm();
    trig.time.year = trig.time.month = trig.time.day = trig.time.hour = trig.time.minute = trig.time.second = 0;
    trig.duration.is_neg = true;
    trig.duration.days = trig.duration.weeks = trig.duration.hours = trig.duration.seconds = 0;
    trig.duration.minutes = 1;
    xprop = icalproperty_new_trigger( trig );
    icalcomponent_add_property( valarm, xprop );
    icalcomponent_add_component( icalevent, valarm );

    icalfileset_add_component(stream,icalcalendar);

	if( icalfileset_commit(stream) != ICAL_NO_ERROR ) {
        #ifdef ICAL_DEBUG
	    printf( "oeICalImpl::Test() : WARNING icalfileset_commit() unsuccessful\n" );
        #endif
    }
#ifdef ICAL_DEBUG
	printf("Appended Event\n");
#endif
    
    icalcomponent *fetchedcal = icalfileset_fetch( stream, uidstr );
    if ( !fetchedcal ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: Cannot fetch event!\n" );
        #endif
        return NS_OK;
    }
	
    icalcomponent *fetchedevent = icalcomponent_get_first_component( fetchedcal,ICAL_VEVENT_COMPONENT);
    if ( !fetchedevent ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: VEVENT not found!\n" );
        #endif
        return NS_OK;
    }

#ifdef ICAL_DEBUG
    printf("Fetched Event\n");
#endif
	
    icalproperty *tmpprop;
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_UID_PROPERTY );
    if ( !tmpprop ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: UID not found!\n" );
        #endif
        return NS_OK;
    }
#ifdef ICAL_DEBUG
    printf("id: %s\n", icalproperty_get_uid( tmpprop ) );
#endif
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_SUMMARY_PROPERTY );
    if ( !tmpprop ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: SUMMARY not found!\n" );
        #endif
        return NS_OK;
    }
#ifdef ICAL_DEBUG
    printf("Title: %s\n", icalproperty_get_summary( tmpprop ) );
#endif
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_CATEGORIES_PROPERTY );
    if ( !tmpprop ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: CATEGORIES not found!\n" );
        #endif
        return NS_OK;
    }
#ifdef ICAL_DEBUG
    printf("Category: %s\n", icalproperty_get_categories( tmpprop ) );
#endif
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_DESCRIPTION_PROPERTY );
    if ( !tmpprop ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: DESCRIPTION not found!\n" );
        #endif
        return NS_OK;
    }
#ifdef ICAL_DEBUG
    printf("Description: %s\n", icalproperty_get_description( tmpprop ) );
#endif
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_LOCATION_PROPERTY );
    if ( !tmpprop ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: LOCATION not found!\n" );
        #endif
        return NS_OK;
    }
#ifdef ICAL_DEBUG
    printf("Location: %s\n", icalproperty_get_location( tmpprop ) );
#endif
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_CLASS_PROPERTY );
    if ( !tmpprop ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: CLASS not found!\n" );
        #endif
        return NS_OK;
    }
#ifdef ICAL_DEBUG
    printf("Class: %s\n", (icalproperty_get_class( tmpprop ) == ICAL_CLASS_PUBLIC) ? "PUBLIC" : "PRIVATE" );
#endif
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_DTSTART_PROPERTY );
    if ( !tmpprop ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: DTSTART not found!\n" );
        #endif
        return NS_OK;
    }
    start = icalproperty_get_dtstart( tmpprop );
#ifdef ICAL_DEBUG
    printf("Start: %d-%d-%d %d:%d\n", start.year, start.month, start.day, start.hour, start.minute );
#endif
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_DTEND_PROPERTY );
    if ( !tmpprop ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: DTEND not found!\n" );
        #endif
        return NS_OK;
    }
    end = icalproperty_get_dtstart( tmpprop );
#ifdef ICAL_DEBUG
    printf("End: %d-%d-%d %d:%d\n", end.year, end.month, end.day, end.hour, end.minute );
#endif
    //
    for( tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_X_PROPERTY );
            tmpprop != 0 ;
            tmpprop = icalcomponent_get_next_property( fetchedevent, ICAL_X_PROPERTY ) ) {
#ifdef ICAL_DEBUG
            icalparameter *tmppar = icalproperty_get_first_parameter( tmpprop, ICAL_MEMBER_PARAMETER );
            printf("%s: %s\n", icalparameter_get_member( tmppar ), icalproperty_get_value_as_string( tmpprop ) );
#endif
    }

    icalcomponent *newcomp = icalcomponent_new_clone( fetchedcal );
    if ( !newcomp ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: Cannot clone event!\n" );
        #endif
        return NS_OK;
    }
    icalcomponent *newevent = icalcomponent_get_first_component( newcomp, ICAL_VEVENT_COMPONENT );
    if ( !tmpprop ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: VEVENT not found!\n" );
        #endif
        return NS_OK;
    }
    tmpprop = icalcomponent_get_first_property( newevent, ICAL_SUMMARY_PROPERTY );
    if ( !tmpprop ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: SUMMARY not found!\n" );
        #endif
        return NS_OK;
    }
    icalproperty_set_summary( tmpprop, "LUNCH AND LEARN TIME" );
//    icalfileset_modify( stream, fetchedcal, newcomp );
    icalfileset_remove_component( stream, fetchedcal );
    icalfileset_add_component( stream, newcomp );
    icalcomponent_free( fetchedcal );
    //

#ifdef ICAL_DEBUG
	printf("Event updated\n");
    printf( "New Title: %s\n", icalproperty_get_summary( tmpprop ) );
#endif

    fetchedcal = icalfileset_fetch( stream, uidstr );
    if ( !fetchedcal ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: Cannot fetch event!\n" );
        #endif
        return NS_OK;
    }
    icalfileset_remove_component( stream, fetchedcal );
	
#ifdef ICAL_DEBUG
    printf("Removed Event\n");
#endif

	if( icalfileset_commit(stream) != ICAL_NO_ERROR ) {
        #ifdef ICAL_DEBUG
	    printf( "oeICalImpl::Test() : WARNING icalfileset_commit() unsuccessful\n" );
        #endif
    }
    icalfileset_free(stream);
    return NS_OK;                                                                    
}

/**
*
*   SetServer
*
*   DESCRIPTION: Sets the Calendar address string of the server.
*   PARAMETERS: 
*	str	- The calendar address string
*
*   RETURN
*      None
*/
NS_IMETHODIMP
oeICalImpl::SetServer( const char *str ) {
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::SetServer(%s)\n", str );
#endif

    if( strncmp( str, "file:///", strlen( "file:///" ) ) == 0 ) {
        nsCOMPtr<nsIURL> url( do_CreateInstance(NS_STANDARDURL_CONTRACTID) );
        nsCString filePath;
        filePath = str;
        url->SetSpec( filePath );
        url->GetFilePath( filePath );
        strcpy( serveraddr, filePath.get() );
        NS_UnescapeURL( serveraddr );
    } else
	    strcpy( serveraddr, str );

    icalfileset *stream;
    
    stream = icalfileset_new(serveraddr);
    if ( !stream ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::SetServer() failed: Cannot open stream: %s!\n", serveraddr );
        #endif
        return NS_OK;
    }
    
    nsresult rv;
    icalcomponent *vcalendar;
    icalcomponent *vevent;
    oeICalEventImpl *icalevent;
    for( vcalendar = icalfileset_get_first_component( stream );
        vcalendar != 0;
        vcalendar = icalfileset_get_next_component( stream ) ) {
        
        for( vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
            vevent != 0;
            vevent = icalcomponent_get_next_component( vcalendar, ICAL_VEVENT_COMPONENT ) ) {

            if( NS_FAILED( rv = NS_NewICalEvent((oeIICalEvent**) &icalevent ))) {
                return rv;
            }
            if( icalevent->ParseIcalComponent( vevent ) ) {
                m_eventlist.Add( icalevent );
            } else {
                icalevent->Release();
            }
        }
    }
    
    icalfileset_free(stream);
    
    for( unsigned int i=0; i<m_observerlist.size(); i++ ) {
        m_observerlist[i]->OnLoad();
    }
    
    SetupAlarmManager();

    return NS_OK;
}


/* attribute boolean batchMode; */
NS_IMETHODIMP oeICalImpl::GetBatchMode(PRBool *aBatchMode)
{
    *aBatchMode = m_batchMode;

    return NS_OK;
}


NS_IMETHODIMP oeICalImpl::SetBatchMode(PRBool aBatchMode)
{
    bool newBatchMode = aBatchMode;

    if( m_batchMode != newBatchMode )
    {
        m_batchMode = aBatchMode;

        // if batch mode changed to off, call set up alarm mamnager

        if( !m_batchMode )
        {
            SetupAlarmManager();
        }
        
        // tell observers about the change

        for( unsigned int i=0; i<m_observerlist.size(); i++ ) 
        {
            if( m_batchMode )
                m_observerlist[i]->OnStartBatch();
            else
                m_observerlist[i]->OnEndBatch();

        }
    }
    
    return NS_OK;
}


/**
*
*   AddEvent
*
*   DESCRIPTION: Adds an event
*
*   PARAMETERS: 
*	icalcalendar - The XPCOM component representing an event
*	retid   - Place to store the returned id
*
*   RETURN
*      None
*/
NS_IMETHODIMP oeICalImpl::AddEvent(oeIICalEvent *icalevent,char **retid)
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::AddEvent()\n" );
#endif
    icalfileset *stream;
    icalcomponent *vcalendar,*fetchedcal;

    stream = icalfileset_new(serveraddr);
    if ( !stream ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::AddEvent() failed: Cannot open stream: %s!\n", serveraddr );
        #endif
        return NS_OK;
    }
    
    ((oeICalEventImpl *)icalevent)->GetId( retid );

    if( *retid == nsnull ) {
        char uidstr[10];
        do {
            unsigned long newid;
            newid = 900000000 + ((rand()%0x4ff) << 16) + rand()%0xFFFF;
            sprintf( uidstr, "%lu", newid );
        } while ( (fetchedcal = icalfileset_fetch( stream, uidstr )) != 0 );

        ((oeICalEventImpl *)icalevent)->SetId( uidstr );
        ((oeICalEventImpl *)icalevent)->GetId( retid );
    }

    vcalendar = ((oeICalEventImpl *)icalevent)->AsIcalComponent();
	
    icalfileset_add_component( stream, vcalendar );

	if( icalfileset_commit(stream) != ICAL_NO_ERROR ) {
        #ifdef ICAL_DEBUG
	    printf( "oeICalImpl::AddEvent() : WARNING icalfileset_commit() unsuccessful\n" );
        #endif
    }
    icalfileset_free( stream );

    icalevent->AddRef();
    m_eventlist.Add( icalevent );

    for( unsigned int i=0; i<m_observerlist.size(); i++ ) {
        m_observerlist[i]->OnAddItem( icalevent );
    }

    SetupAlarmManager();
    return NS_OK;
}

/**
*
*   ModifyEvent
*
*   DESCRIPTION: Updates an event
*
*   PARAMETERS: 
*	icalcalendar	- The XPCOM component representing an event
*	retid   - Place to store the returned id
*
*   RETURN
*      None
*/
NS_IMETHODIMP oeICalImpl::ModifyEvent(oeIICalEvent *icalevent, char **retid)
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::ModifyEvent()\n" );
#endif
    icalfileset *stream;
    icalcomponent *vcalendar;

    stream = icalfileset_new(serveraddr);
    if ( !stream ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::ModifyEvent() failed: Cannot open stream: %s!\n", serveraddr );
        #endif
        return NS_OK;
    }
    
    icalevent->GetId( retid );
    if( *retid == nsnull ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::ModifyEvent() - Invalid Id.\n" );
        #endif
        icalfileset_free(stream);
        return NS_OK;
    }
    
    icalcomponent *fetchedvcal = icalfileset_fetch( stream, *retid );
    
    oeICalEventImpl *oldevent=nsnull;
    if( fetchedvcal ) {
        icalcomponent *fetchedvevent = icalcomponent_fetch( fetchedvcal, *retid );
        if( fetchedvevent ) {
            icalcomponent_remove_component( fetchedvcal, fetchedvevent );
            if ( !icalcomponent_get_first_real_component( fetchedvcal ) ) {
                icalfileset_remove_component( stream, fetchedvcal );
                icalcomponent_free( fetchedvcal );
            }
            nsresult rv;
            if( NS_FAILED( rv = NS_NewICalEvent((oeIICalEvent**) &oldevent ))) {
                nsMemory::Free( *retid );
                *retid = nsnull;
                icalfileset_free(stream);
                return rv;
            }
            oldevent->ParseIcalComponent( fetchedvevent );
            icalcomponent_free( fetchedvevent );
        } else {
            #ifdef ICAL_DEBUG
            printf( "oeICalImpl::ModifyEvent() - WARNING Event not found.\n" );
            #endif
            nsMemory::Free( *retid );
            *retid = nsnull;
            icalfileset_free(stream);
            return NS_OK;
        }
    } else {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::ModifyEvent() - WARNING Event not found.\n" );
        #endif
        nsMemory::Free( *retid );
        *retid = nsnull;
        icalfileset_free(stream);
        return NS_OK;
    }
    
    vcalendar = ((oeICalEventImpl *)icalevent)->AsIcalComponent();
    icalfileset_add_component( stream, vcalendar );
    
	if( icalfileset_commit(stream) != ICAL_NO_ERROR ) {
        #ifdef ICAL_DEBUG
	    printf( "oeICalImpl::ModifyEvent() : WARNING icalfileset_commit() unsuccessful\n" );
        #endif
    }
    icalfileset_free(stream);
    
    for( unsigned int i=0; i<m_observerlist.size(); i++ ) {
        m_observerlist[i]->OnModifyItem( icalevent, oldevent );
    }

    oldevent->Release();

    SetupAlarmManager();
    return NS_OK;
}


/**
*
*   FetchEvent
*
*   DESCRIPTION: Fetches an event
*
*   PARAMETERS: 
*	ev	- Place to store the XPCOM representation of the fetched event
*	id      - Id of the event to fetch
*
*   RETURN
*      None
*/
NS_IMETHODIMP oeICalImpl::FetchEvent( const char *id, oeIICalEvent **ev)
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalImpl::FetchEvent()\n" );
#endif
    oeIICalEvent* event = m_eventlist.GetEventById( id );
    if( event != nsnull ) {
        event->AddRef();
    }
    *ev = event;
    return NS_OK;
}

/**
*
*   DeleteEvent
*
*   DESCRIPTION: Deletes an event
*
*   PARAMETERS: 
*	id		- Id of the event to delete
*
*   RETURN
*      None
*/
NS_IMETHODIMP
oeICalImpl::DeleteEvent( const char *id )
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::DeleteEvent( %s )\n", id );
#endif
    icalfileset *stream;
    
    stream = icalfileset_new(serveraddr);
    if ( !stream ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::DeleteEvent() failed: Cannot open stream: %s!\n", serveraddr );
        #endif
        return NS_OK;
    }
    
    if( id == nsnull ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::DeleteEvent() - Invalid Id.\n" );
        #endif
        icalfileset_free(stream);
        return NS_OK;
    }

    icalcomponent *fetchedvcal = icalfileset_fetch( stream, id );
    
    if( !fetchedvcal ) {
        icalfileset_free(stream);
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::DeleteEvent() - WARNING Event not found.\n" );
        #endif
        return NS_OK;
    }
    
    icalcomponent *fetchedvevent = icalcomponent_fetch( fetchedvcal, id );
    
    if( !fetchedvevent ) {
        icalfileset_free(stream);
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::DeleteEvent() - WARNING Event not found.\n" );
        #endif
        return NS_OK;
    }

    icalcomponent_remove_component( fetchedvcal, fetchedvevent );
    icalcomponent_free( fetchedvevent );
    if ( !icalcomponent_get_first_real_component( fetchedvcal ) ) {
        icalfileset_remove_component( stream, fetchedvcal );
        icalcomponent_free( fetchedvcal );
    }
	
    icalfileset_mark( stream ); //Make sure stream is marked as dirty
	if( icalfileset_commit(stream) != ICAL_NO_ERROR ) {
        #ifdef ICAL_DEBUG
	    printf( "oeICalImpl::ModifyEvent() : WARNING icalfileset_commit() unsuccessful\n" );
        #endif
    }
    icalfileset_free(stream);
	
    oeIICalEvent *icalevent;
    FetchEvent( id , &icalevent );

    m_eventlist.Remove( id );
    
    for( unsigned int i=0; i<m_observerlist.size(); i++ ) {
        m_observerlist[i]->OnDeleteItem( icalevent );
    }

    icalevent->Release();

    SetupAlarmManager();
	return NS_OK;
}
/*
NS_IMETHODIMP
oeICalImpl::GetAllEvents(nsISimpleEnumerator **resultList )
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::GetAllEvents()\n" );
#endif
    nsCOMPtr<oeEventEnumerator> eventEnum = new oeEventEnumerator();
    
    if (!eventEnum)
        return NS_ERROR_OUT_OF_MEMORY;

    EventList *tmplistptr = &m_eventlist;
    while( tmplistptr ) {
        if( tmplistptr->event ) {
            eventEnum->AddEvent( tmplistptr->event );
        }
        tmplistptr = tmplistptr->next;
    }

    // bump ref count
    return eventEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)resultList);
}*/
NS_IMETHODIMP
oeICalImpl::GetAllEvents(nsISimpleEnumerator **resultList )
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::GetAllEvents()\n" );
#endif
    nsCOMPtr<oeEventEnumerator> eventEnum = new oeEventEnumerator();
    
    if (!eventEnum)
        return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsISupportsArray> eventArray;
    NS_NewISupportsArray(getter_AddRefs(eventArray));
    if (eventArray == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    EventList *tmplistptr = &m_eventlist;
    while( tmplistptr ) {
        if( tmplistptr->event ) {
            eventArray->AppendElement( tmplistptr->event );
        }
        tmplistptr = tmplistptr->next;
    }

    PRTime todayinms = PR_Now();
    PRInt64 usecpermsec;
    LL_I2L( usecpermsec, PR_USEC_PER_MSEC );
    LL_DIV( todayinms, todayinms, usecpermsec );

    struct icaltimetype checkdate = ConvertFromPrtime( todayinms );
    struct icaltimetype now = ConvertFromPrtime( todayinms );
    icaltime_adjust( &now, 0, 0, 0, -1 );

    icaltimetype nextcheckdate;
    PRUint32 num;

    do {
        icaltimetype soonest = icaltime_null_time();
        eventArray->Count( &num );
        for ( unsigned int i=0; i<num; i++ ) {
            oeIICalEvent* tmpevent;
            eventArray->GetElementAt( i, (nsISupports **)&tmpevent );
            icaltimetype next = ((oeICalEventImpl *)tmpevent)->GetNextRecurrence( now );
            if( !icaltime_is_null_time( next ) )
                continue;
            icaltimetype previous = ((oeICalEventImpl *)tmpevent)->GetPreviousOccurrence( checkdate );
            if( !icaltime_is_null_time( previous ) && ( icaltime_is_null_time( soonest ) || (icaltime_compare( soonest, previous ) > 0) ) ) {
                soonest = previous;
            }
        }

        nextcheckdate = soonest;
        if( !icaltime_is_null_time( nextcheckdate )) {

            for ( unsigned int i=0; i<num; i++ ) {
                oeIICalEvent* tmpevent;
                eventArray->GetElementAt( i, (nsISupports **)&tmpevent );
                icaltimetype next = ((oeICalEventImpl *)tmpevent)->GetNextRecurrence( now );
                if( !icaltime_is_null_time( next ) )
                    continue;
                icaltimetype previous = ((oeICalEventImpl *)tmpevent)->GetPreviousOccurrence( checkdate );
                if( !icaltime_is_null_time( previous ) && (icaltime_compare( nextcheckdate, previous ) == 0) ) {
                    eventEnum->AddEvent( tmpevent );
//                    PRTime nextdateinms = ConvertToPrtime( nextcheckdate );
//                    dateEnum->AddDate( nextdateinms );
                    eventArray->RemoveElementAt( i );
                    break;
                }
            }
        }
    } while ( !icaltime_is_null_time( nextcheckdate ) );

    checkdate = ConvertFromPrtime( todayinms );
    icaltime_adjust( &checkdate, 0, 0, 0, -1 );
    
    do {

        icaltimetype soonest = icaltime_null_time();
        eventArray->Count( &num );
        for ( unsigned int i=0; i<num; i++ ) {
            oeIICalEvent* tmpevent;
            eventArray->GetElementAt( i, (nsISupports **)&tmpevent );
            icaltimetype next = ((oeICalEventImpl *)tmpevent)->GetNextRecurrence( checkdate );
            if( !icaltime_is_null_time( next ) && ( icaltime_is_null_time( soonest ) || (icaltime_compare( soonest, next ) > 0) ) ) {
                soonest = next;
            }
        }

        nextcheckdate = soonest;

        if( !icaltime_is_null_time( nextcheckdate )) {

            for ( unsigned int i=0; i<num; i++ ) {
                oeIICalEvent* tmpevent;
                eventArray->GetElementAt( i, (nsISupports **)&tmpevent );
                icaltimetype next = ((oeICalEventImpl *)tmpevent)->GetNextRecurrence( checkdate );
                if( !icaltime_is_null_time( next ) && (icaltime_compare( nextcheckdate, next ) == 0) ) {
                    eventEnum->AddEvent( tmpevent );
//                    PRTime nextdateinms = ConvertToPrtime( nextcheckdate );
//                    dateEnum->AddDate( nextdateinms );
                    eventArray->RemoveElementAt( i );
                    icaltime_adjust( &nextcheckdate, 0, 0, 0, -1 );
                    break;
                }
            }
            checkdate = nextcheckdate;
        }
    } while ( !icaltime_is_null_time( nextcheckdate ) );


    // bump ref count
    return eventEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)resultList);
}

NS_IMETHODIMP
oeICalImpl::SearchBySQL( const char *sqlstr,  nsISimpleEnumerator **resultList )
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::SearchBySQL()\n" );
#endif
    icalgauge* gauge;
    icalfileset *stream;
    
    nsCOMPtr<oeEventEnumerator> eventEnum = new oeEventEnumerator( );
    
    if (!eventEnum)
        return NS_ERROR_OUT_OF_MEMORY;

    stream = icalfileset_new(serveraddr);
    if ( !stream ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::SearchBySQL() failed: Cannot open stream: %s!\n", serveraddr );
        #endif
        return NS_OK;
    }
    
    gauge = icalgauge_new_from_sql( (char *)sqlstr );
    if ( !gauge ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::SearchBySQL() failed: Cannot create gauge\n" );
        #endif
        return NS_OK;
    }
    
    icalfileset_select( stream, gauge );

    icalcomponent *comp;
    for( comp = icalfileset_get_first_component( stream );
        comp != 0;
        comp = icalfileset_get_next_component( stream ) ) {
        icalcomponent *vevent = icalcomponent_get_first_component( comp, ICAL_VEVENT_COMPONENT );
        if ( !vevent ) {
            #ifdef ICAL_DEBUG
            printf( "oeICalImpl::SearchBySQL() failed: VEVENT not found!\n" );
            #endif
            return NS_OK;
        }

        icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_UID_PROPERTY );
        if ( !prop ) {
            #ifdef ICAL_DEBUG
            printf( "oeICalImpl::SearchBySQL() failed: UID not found!\n" );
            #endif
            return NS_OK;
        }

        oeIICalEvent* event = m_eventlist.GetEventById( icalproperty_get_uid( prop ) );
        if( event != nsnull ) {
            eventEnum->AddEvent( event );
        }
    }
	
    icalfileset_free(stream);
    
    // bump ref count
    return eventEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)resultList);
}

NS_IMETHODIMP
oeICalImpl::GetEventsForMonth( PRTime datems, nsISimpleEnumerator **datelist, nsISimpleEnumerator **eventlist ) {
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::GetEventsForMonth()\n" );
#endif
    struct icaltimetype checkdate = ConvertFromPrtime( datems );
    checkdate.day = 1;
    checkdate.hour = 0;
    checkdate.minute = 0;
    checkdate.second = 0;
    icaltime_adjust( &checkdate, 0, 0 , 0, -1 );
#ifdef ICAL_DEBUG_ALL
    printf( "CHECKDATE: %s\n" , icaltime_as_ctime( checkdate ) );
#endif
    PRTime checkdateinms = ConvertToPrtime( checkdate );

    struct icaltimetype checkenddate = ConvertFromPrtime( datems );
    checkenddate.month++;
    checkenddate.day = 1;
    checkenddate.hour = 0;
    checkenddate.minute = 0;
    checkenddate.second = 0;
    checkenddate = icaltime_normalize( checkenddate );
#ifdef ICAL_DEBUG_ALL
    printf( "CHECKENDDATE: %s\n" , icaltime_as_ctime( checkenddate ) );
#endif
    PRTime checkenddateinms = ConvertToPrtime( checkenddate );

    return GetEventsForRange(checkdateinms ,checkenddateinms ,datelist ,eventlist );
}

NS_IMETHODIMP
oeICalImpl::GetEventsForWeek( PRTime datems, nsISimpleEnumerator **datelist, nsISimpleEnumerator **eventlist ) {
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::GetEventsForWeek()\n" );
#endif

    struct icaltimetype checkdate = ConvertFromPrtime( datems );
    checkdate.hour = 0;
    checkdate.minute = 0;
    checkdate.second = 0;
    icaltime_adjust( &checkdate, 0, 0 , 0, -1 );
    PRTime checkdateinms = ConvertToPrtime( checkdate );

    struct icaltimetype checkenddate = ConvertFromPrtime( datems );
    checkenddate.hour = 0;
    checkenddate.minute = 0;
    checkenddate.second = 0;
    icaltime_adjust( &checkenddate, 7, 0 , 0, 0 );
    PRTime checkenddateinms = ConvertToPrtime( checkenddate );

    return GetEventsForRange(checkdateinms ,checkenddateinms ,datelist ,eventlist );
}

NS_IMETHODIMP
oeICalImpl::GetEventsForDay( PRTime datems, nsISimpleEnumerator **datelist, nsISimpleEnumerator **eventlist ) {
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::GetEventsForDay()\n" );
#endif

    struct icaltimetype checkdate = ConvertFromPrtime( datems );
    checkdate.hour = 0;
    checkdate.minute = 0;
    checkdate.second = 0;
    icaltime_adjust( &checkdate, 0, 0, 0, -1 );
    PRTime checkdateinms = ConvertToPrtime( checkdate );

    struct icaltimetype checkenddate = ConvertFromPrtime( datems );
    checkenddate.hour = 0;
    checkenddate.minute = 0;
    checkenddate.second = 0;
    icaltime_adjust( &checkenddate, 1, 0, 0, 0 );
    PRTime checkenddateinms = ConvertToPrtime( checkenddate );

    return GetEventsForRange(checkdateinms ,checkenddateinms ,datelist ,eventlist );
}
/*
NS_IMETHODIMP
oeICalImpl::GetEventsForRange( PRTime checkdateinms, PRTime checkenddateinms, nsISimpleEnumerator **datelist, nsISimpleEnumerator **eventlist ) {
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalImpl::GetEventsForRange()\n" );
#endif
    
    nsCOMPtr<oeEventEnumerator> eventEnum = new oeEventEnumerator( );
    
    if (!eventEnum)
        return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<oeDateEnumerator> dateEnum = new oeDateEnumerator( );
    
    if (!dateEnum)
        return NS_ERROR_OUT_OF_MEMORY;

    EventList *tmplistptr = &m_eventlist;
    while( tmplistptr ) {
        if( tmplistptr->event ) {
            oeIICalEvent* tmpevent = tmplistptr->event;
            PRBool isvalid;
            PRTime checkdateloop = checkdateinms;
            tmpevent->GetNextRecurrence( checkdateloop, &checkdateloop, &isvalid );
            while( isvalid && LL_CMP( checkdateloop, <, checkenddateinms ) ) {
                eventEnum->AddEvent( tmpevent );
                dateEnum->AddDate( checkdateloop );
                tmpevent->GetNextRecurrence( checkdateloop, &checkdateloop, &isvalid );
            }
        }
        tmplistptr = tmplistptr->next;
    }

    eventEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)eventlist);
    dateEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)datelist);
    return NS_OK;
}*/

NS_IMETHODIMP
oeICalImpl::GetEventsForRange( PRTime checkdateinms, PRTime checkenddateinms, nsISimpleEnumerator **datelist, nsISimpleEnumerator **eventlist ) {
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalImpl::GetEventsForRange()\n" );
#endif
    
    nsCOMPtr<oeEventEnumerator> eventEnum = new oeEventEnumerator( );
    
    if (!eventEnum)
        return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<oeDateEnumerator> dateEnum = new oeDateEnumerator( );
    
    if (!dateEnum)
        return NS_ERROR_OUT_OF_MEMORY;

    struct icaltimetype checkdate = ConvertFromPrtime( checkdateinms );
    icaltime_adjust( &checkdate, 0, 0, 0, -1 );
    
    struct icaltimetype checkenddate = ConvertFromPrtime( checkenddateinms );

    icaltimetype nextcheckdate;
    do {
        nextcheckdate = GetNextEvent( checkdate );
        if( icaltime_compare( nextcheckdate, checkenddate ) >= 0 )
            break;
        if( !icaltime_is_null_time( nextcheckdate )) {
            EventList *tmplistptr = &m_eventlist;
            while( tmplistptr ) {
                if( tmplistptr->event ) {
                    oeIICalEvent* tmpevent = tmplistptr->event;
                    icaltimetype next = ((oeICalEventImpl *)tmpevent)->GetNextRecurrence( checkdate );
                    if( !icaltime_is_null_time( next ) && (icaltime_compare( nextcheckdate, next ) == 0) ) {
                        eventEnum->AddEvent( tmpevent );
                        PRTime nextdateinms = ConvertToPrtime( nextcheckdate );
                        dateEnum->AddDate( nextdateinms );
                    }
                }
                tmplistptr = tmplistptr->next;
            }
            checkdate = nextcheckdate;
        }
    } while ( !icaltime_is_null_time( nextcheckdate ) );

    eventEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)eventlist);
    dateEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)datelist);
    return NS_OK;
}
/*
NS_IMETHODIMP
oeICalImpl::GetFirstEventsForRange( PRTime checkdateinms, PRTime checkenddateinms, nsISimpleEnumerator **eventlist ) {
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalImpl::GetFirstEventsForRange()\n" );
#endif
    
    nsCOMPtr<oeEventEnumerator> eventEnum = new oeEventEnumerator( );
    
    if (!eventEnum)
        return NS_ERROR_OUT_OF_MEMORY;

    EventList *tmplistptr = &m_eventlist;
    while( tmplistptr ) {
        if( tmplistptr->event ) {
            oeIICalEvent* tmpevent = tmplistptr->event;
            PRBool isvalid;
            PRTime checkdateloop = checkdateinms;
            tmpevent->GetNextRecurrence( checkdateloop, &checkdateloop, &isvalid );
            if( isvalid && LL_CMP( checkdateloop, <, checkenddateinms ) ) {
                eventEnum->AddEvent( tmpevent );
            }
        }
        tmplistptr = tmplistptr->next;
    }

    eventEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)eventlist);
    return NS_OK;
}*/
NS_IMETHODIMP
oeICalImpl::GetFirstEventsForRange( PRTime checkdateinms, PRTime checkenddateinms, nsISimpleEnumerator **eventlist ) {
//NOTE: checkenddateinms is being ignored for now
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalImpl::GetFirstEventsForRange()\n" );
#endif
    nsCOMPtr<oeEventEnumerator> eventEnum = new oeEventEnumerator();
    
    if (!eventEnum)
        return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsISupportsArray> eventArray;
    NS_NewISupportsArray(getter_AddRefs(eventArray));
    if (eventArray == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    EventList *tmplistptr = &m_eventlist;
    while( tmplistptr ) {
        if( tmplistptr->event ) {
            eventArray->AppendElement( tmplistptr->event );
        }
        tmplistptr = tmplistptr->next;
    }

    struct icaltimetype checkdate = ConvertFromPrtime( checkdateinms );
    icaltime_adjust( &checkdate, 0, 0, 0, -1 );

    icaltimetype nextcheckdate;
    do {
        PRUint32 num;
        icaltimetype soonest = icaltime_null_time();
        eventArray->Count( &num );
        for ( unsigned int i=0; i<num; i++ ) {
            oeIICalEvent* tmpevent;
            eventArray->GetElementAt( i, (nsISupports **)&tmpevent );
            icaltimetype next = ((oeICalEventImpl *)tmpevent)->GetNextRecurrence( checkdate );
            if( !icaltime_is_null_time( next ) && ( icaltime_is_null_time( soonest ) || (icaltime_compare( soonest, next ) > 0) ) ) {
                soonest = next;
            }
        }

        nextcheckdate = soonest;

        if( !icaltime_is_null_time( nextcheckdate )) {

            for ( unsigned int i=0; i<num; i++ ) {
                oeIICalEvent* tmpevent;
                eventArray->GetElementAt( i, (nsISupports **)&tmpevent );
                icaltimetype next = ((oeICalEventImpl *)tmpevent)->GetNextRecurrence( checkdate );
                if( !icaltime_is_null_time( next ) && (icaltime_compare( nextcheckdate, next ) == 0) ) {
                    eventEnum->AddEvent( tmpevent );
//                    PRTime nextdateinms = ConvertToPrtime( nextcheckdate );
//                    dateEnum->AddDate( nextdateinms );
                    eventArray->RemoveElementAt( i );
                    icaltime_adjust( &nextcheckdate, 0, 0, 0, -1 );
                    break;
                }
            }
            checkdate = nextcheckdate;
        }
    } while ( !icaltime_is_null_time( nextcheckdate ) );


    // bump ref count
    return eventEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)eventlist);
}

icaltimetype oeICalImpl::GetNextEvent( icaltimetype starting ) {
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalImpl::GetNextEvent()\n" );
#endif
    icaltimetype soonest = icaltime_null_time();
    
    EventList *tmplistptr = &m_eventlist;
    while( tmplistptr ) {
        if( tmplistptr->event ) {
            oeIICalEvent* tmpevent = tmplistptr->event;
            icaltimetype next = ((oeICalEventImpl *)tmpevent)->GetNextRecurrence( starting );
            if( !icaltime_is_null_time( next ) && ( icaltime_is_null_time( soonest ) || (icaltime_compare( soonest, next ) > 0) ) ) {
                soonest = next;
            }
        }
        tmplistptr = tmplistptr->next;
    }
    return soonest;
}

NS_IMETHODIMP
oeICalImpl::GetNextNEvents( PRTime datems, PRInt32 maxcount, nsISimpleEnumerator **datelist, nsISimpleEnumerator **eventlist ) {
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::GetNextNEvents( %d )\n", maxcount );
#endif
    
    nsCOMPtr<oeEventEnumerator> eventEnum = new oeEventEnumerator( );
    
    if (!eventEnum)
        return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<oeDateEnumerator> dateEnum = new oeDateEnumerator( );
    
    if (!dateEnum)
        return NS_ERROR_OUT_OF_MEMORY;

    struct icaltimetype checkdate = ConvertFromPrtime( datems );
    icaltime_adjust( &checkdate, 0, 0, 0, -1 );
    
    int count = 0;
    icaltimetype nextcheckdate;
    do {
        nextcheckdate = GetNextEvent( checkdate );
        if( !icaltime_is_null_time( nextcheckdate )) {
            EventList *tmplistptr = &m_eventlist;
            while( tmplistptr && count<maxcount ) {
                if( tmplistptr->event ) {
                    oeIICalEvent* tmpevent = tmplistptr->event;
                    icaltimetype next = ((oeICalEventImpl *)tmpevent)->GetNextRecurrence( checkdate );
                    if( !icaltime_is_null_time( next ) && (icaltime_compare( nextcheckdate, next ) == 0) ) {
                        eventEnum->AddEvent( tmpevent );
                        PRTime nextdateinms = ConvertToPrtime( nextcheckdate );
                        dateEnum->AddDate( nextdateinms );
                        count++;
                    }
                }
                tmplistptr = tmplistptr->next;
            }
            checkdate = nextcheckdate;
        }
    } while ( !icaltime_is_null_time( nextcheckdate ) && (count < maxcount) );

    eventEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)eventlist);
    dateEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)datelist);
    return NS_OK;
}

NS_IMETHODIMP 
oeICalImpl::AddObserver(oeIICalObserver *observer)
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::AddObserver()\n" );
#endif
    if( observer ) {
        observer->AddRef();
        m_observerlist.push_back( observer );
        observer->OnLoad();
    }
    return NS_OK;
}

NS_IMETHODIMP 
oeICalImpl::RemoveObserver(oeIICalObserver *observer)
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::RemoveObserver()\n" );
#endif
    if( observer ) {
        for( unsigned int i=0; i<m_observerlist.size(); i++ ) {
            if( observer == m_observerlist[i] ) {
//                m_observerlist.erase( &m_observerlist[i] );
                observer->Release();
                break;
            }
        }
    }
    return NS_OK;
}

void AlarmTimerCallback(nsITimer *aTimer, void *aClosure)
{
#ifdef ICAL_DEBUG
    printf( "AlarmTimerCallback\n" );
#endif
    oeICalImpl *icallib = (oeICalImpl *)aClosure;
    icallib->SetupAlarmManager();
}

void oeICalImpl::SetupAlarmManager() {
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::SetupAlarmManager()\n" );
#endif

    if( m_batchMode )
    {
        #ifdef ICAL_DEBUG
            printf( "oeICalImpl::SetupAlarmManager() - defering alarms while in batch mode\n" );
        #endif
        
        return;
    }

    PRTime todayinms = PR_Now();
    PRInt64 usecpermsec;
    LL_I2L( usecpermsec, PR_USEC_PER_MSEC );
    LL_DIV( todayinms, todayinms, usecpermsec );

    icaltimetype now = ConvertFromPrtime( todayinms );

    icaltimetype nextalarm = icaltime_null_time();
    EventList *tmplistptr = &m_eventlist;
    while( tmplistptr ) {
        oeICalEventImpl *event = (oeICalEventImpl *)(tmplistptr->event);
        if( event ) {
            icaltimetype begin=icaltime_null_time();
            begin.year = 1970; begin.month=1; begin.day=1;
            icaltimetype alarmtime = begin;
            do {
                alarmtime = event->GetNextAlarmTime( alarmtime );
                if( icaltime_is_null_time( alarmtime ) )
                    break;
                if( icaltime_compare( alarmtime, now ) <= 0 ) {
#ifdef ICAL_DEBUG
                    printf( "ALARM WENT OFF: %s\n", icaltime_as_ctime( alarmtime ) );
#endif
                    
                    for( unsigned int i=0; i<m_observerlist.size(); i++ ) {
                        m_observerlist[i]->OnAlarm( event );
                    }
                }
                else {
                    if( icaltime_is_null_time( nextalarm ) )
                        nextalarm = alarmtime;
                    else if( icaltime_compare( nextalarm, alarmtime ) > 0 )
                        nextalarm = alarmtime;
                    break;
                }
            } while ( 1 );
        }
        tmplistptr = tmplistptr->next;
    }
    if( m_alarmtimer  ) {
        if ( m_alarmtimer->GetDelay() != 0 )
            m_alarmtimer->Cancel();
        m_alarmtimer->Release();
        m_alarmtimer = nsnull;
    }
    if( !icaltime_is_null_time( nextalarm ) ) {
#ifdef ICAL_DEBUG
        printf( "NEXT ALARM IS: %s\n", icaltime_as_ctime( nextalarm ) );
#endif
        time_t timediff = icaltime_as_timet( nextalarm ) - icaltime_as_timet( now );
        
        nsresult rv;
        nsCOMPtr<nsITimer> alarmtimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
        alarmtimer->QueryInterface(NS_GET_IID(nsITimer), (void **)&m_alarmtimer);
        if( NS_FAILED( rv ) )
            m_alarmtimer = nsnull;
        else
            m_alarmtimer->Init( AlarmTimerCallback, this, timediff*1000, NS_PRIORITY_NORMAL, NS_TYPE_ONE_SHOT );
    }
}
