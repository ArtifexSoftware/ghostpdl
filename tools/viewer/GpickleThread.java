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
 * @version $Revision$
 */
public class GpickleThread extends Gpickle implements Runnable {

  /** debug printf control */
  private final static boolean debug = true;

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

  /** threads must run, this one runs the process in Gpickle
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
	      // generate page and return it to the observer
	      observer.pageCountIsReady( getPrinterPageCount() );
	      getPageCount = false;
	  } 
	  else {
	      observer.imageIsReady( getPrinterOutputPage() );
	  }

          if (debug) {
             gotItTime = System.currentTimeMillis() - gotItTime;	
             System.out.println("Render time is: " +  gotItTime + " milliseconds" );
          }

          synchronized(this) {
             // run once per page request
             goFlag = false;
             if (nextPage > 0) {
                // the last page asked for while I was busy
                startProduction( nextPage );
             }
          }
        }
     } catch (InterruptedException e) {}
  }

  /** let the thread run one loop
   * looks kind of like the depreciated Thread.resume()
   */
  private synchronized void resume()
  {
     goFlag = true;
     nextPage = 0;
     notify();
  }

  /** if not generating then start generated requested page
   *  if already generating remember last request and return
   */
  public void startProduction( int pageNumber )
  {
     if (debug) System.out.println("Request =" + pageNumber);	

     if (goFlag) {   // one at a time please
        nextPage = pageNumber;
        return;
     }
     setPageNumber(pageNumber);

     if (debug) {
        gotItTime = System.currentTimeMillis();	
        System.out.println("Getting  =" + pageNumber);
     }

     resume(); // kickStartTread
  }  

  /** if not generating then start generated requested pageCount
   */
  public void startCountingPages(  )
  {
     if (debug) System.out.println("Request PageCount");	

     if (goFlag) {   // one at a time please
        return;
     }
     
     getPageCount = true;

     if (debug) {
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
}
