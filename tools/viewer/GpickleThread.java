/* Portions Copyright (C) 2001 Artifex Software Inc.
   
   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

import java.awt.image.*;


/** Thread calls Gpickle asking for a page
 * this tread will send the BufferedImage to
 * the registered PickleObserver
 *
 * startCountingPages() uses the same thread to count the number of pages in a job
 * since it maybe desirable to count while viewing a second GpickeTread is 
 * needed.  One generates pages, the other counts the total number of pages.
 *
 * @author Stefan Kemper
 * @version $Revision: 1645 $
 */
public class GpickleThread extends Gpickle implements Runnable {

  /** debug printf control */
  private final static boolean debug = false;
  private final static boolean debugPerformance = debug && true;
  private final static boolean debugSpew = debug && false;

  private long gotItTime = 0;

  /** call back for generated page */
  private GpickleObserver observer;

  /** they depreciated suspend and resume for Thread,
   * could have used runnable but why not invent the wheel
   */
  private volatile boolean goFlag = false;

  /** if not zero this is the last page requested while
  * we were busy, fetch it when we are not busy.
  */
  private volatile int nextPage = 0;

  /** the page we are fetching now
  */
  private volatile int currentPage = 0;

  /** NB since the pagecount thread is a different object than
  * the page generator the caller must set this in both objects.
  */
  private volatile int totalPageCount = 0;

  /** Enable/disable lookahead fetches.
  * NB would be nice to look back when user is paging backwards.
  */
  private volatile boolean lookAhead = true;
  private final static boolean lookAheadFeatureEnabled = true;

  /** true if getting page count, false if getting a page */
  private volatile boolean getPageCount = false;

  private Thread myThread;

  /** Since I don't construct pickles caller needs to manage the one pickle per thread
  * relationship.
  * PickleObserver.ImageIsReady() will be called with the generated images.
  */
  GpickleThread( GpickleObserver callme )
  {
     observer = callme;

     myThread = new Thread( this );
     myThread.start();
  }

  /** Threads must run, this one runs the process in Gpickle
   *  sends generated page to observer.
   *  one page per request
   */
  public void run()
  {
     try {
        while (true)
        {
          synchronized(this) {
             while(!goFlag) {
                wait();  // block, wait for resume
             }
          }

	  if (getPageCount) {
	      // generate count of pages in document and return it to the observer
	
	      if (debugSpew) System.out.println("Start Counting pages");
	      observer.pageCountIsReady( getPrinterPageCount() );
	      if (debugSpew) System.out.println("Finished Counting pages");
	      synchronized(this) {
		  getPageCount = false;
		  goFlag = false;
	      }
	      continue; // wait for next call, avoiding lookahead logic below.
	  }
	  else {
	      // generate page and return it to the observer
	      observer.imageIsReady( getPrinterOutputPage( currentPage ) );
	  }
          if (debugPerformance) {
	     gotItTime = System.currentTimeMillis() - gotItTime;	
	     System.out.println("Page " + currentPage + " User time: " +
				gotItTime + " milliseconds" );
          }

          synchronized(this) {
             // run once per page request
             goFlag = false;
	
             if (nextPage > 0) {
		// the last page asked for while I was busy
		// holding the the pageDown key down will get you the next page
		// and one far into the document (nextPage)
		if (debugSpew) System.out.println("Getting Next asked for " +  nextPage );
                
		startProduction( nextPage );
             }
	     else if (lookAhead) {
		 if ( currentPage < totalPageCount) {
		     // look ahead fetches the next page into cache, not displayed;
		     // hopefully completes while the user is viewing the current page
		     if (debugSpew)
			 System.out.println("Fetching ahead " +
					    (currentPage + 1) +
					    " totalpages " +
					    totalPageCount);
		     getPrinterOutputPage(currentPage + 1);
		 }
		 else {
		     // if we don't know how many pages, then the page count is still running
		     // wait until its done to start looking ahead.
		     if (debugSpew)
			 System.out.println("Bad totalPageCount" +
					    (currentPage + 1) +
					    " totalpages " +
					    totalPageCount);
		 }
	     }
          }
        }
     } catch (InterruptedException e) {}
  }

  /** Let the thread run one loop.
   * Looks kind of like the depreciated Thread.resume().
   */
  private synchronized void resume()
  {
     goFlag = true;
     nextPage = 0;
     notify();
  }

  /** startProduction with defaulted lookAhead
   */
  public void startProduction( int pageNumber )
  {
      startProduction( pageNumber, lookAheadFeatureEnabled );
  }

  /** if not generating then start generated requested page
   *  if already generating remember last request and return
   *  Set lookAhead flag to prefetch next page into cache.
   */
  public void startProduction( int pageNumber, boolean _lookAhead  )
  {
     if (debugSpew) System.out.println("Request =" + pageNumber +
				       " lookAhead " + _lookAhead );	

     if (goFlag) {   // one at a time please
        nextPage = pageNumber;
        return;
     }
     currentPage = pageNumber;

     if (debugPerformance) {
        gotItTime = System.currentTimeMillis();	
     }

     // look ahead flag
     lookAhead = _lookAhead && lookAheadFeatureEnabled;

     resume(); // kickStartTread
  }

  /** if not generating then start generated requested pageCount
   */
  public void startCountingPages(  )
  {
     if (debugSpew) System.out.println("Request PageCount");	
     if (goFlag) {   // one at a time please
	 if (debugSpew) System.out.println("Request PageCount goFlag true");	
        return;
     }
     getPageCount = true;

     if (debugSpew) {
        gotItTime = System.currentTimeMillis();	
        System.out.println("Getting Page Count");
     }

     resume(); // kickStartTread
  }

  public boolean busy() {
     if (nextPage > 0)
        return true;
     return false;
  }

  /** Since muliple threads can view a single document, the caller must
  * set the number of pages in the document for all threads.
  * This page count is used to prevent look ahead past the end of document.
  * It doesn't prevent the caller from doing this. 
  */
  public void setPageCount( int pageCount ) {
      totalPageCount = pageCount;
  }
}
