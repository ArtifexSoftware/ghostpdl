/* Portions Copyright (C) 2001 Artifex Software Inc.
   
   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

import java.awt.*;
import java.awt.image.*;
import java.io.*;
import java.util.*;
import com.sun.image.codec.jpeg.*;

/** Interface to pcl/pxl interpreter
 * Uses Process to interpret and pipe the results back.
 * @author Henry Stiles
 * @version $Revision$
*/

public class Gpickle {
    /** debug printf control */
    private static boolean debug = false;
    /** here are some defaults that might be overriden will probably be
    * overriden.
    */
    private double xRes = 75f;
    private double yRes = 75f;
    private String jobList = "startpage.pcl";
    protected int pageToDisplay = 1;
    /** This is a local counter */
    private volatile int myTotalPageCount = -1;  // defaults to error

    /** We always use jpeg for output because java has good internal support for jpeg.
     */
    private String deviceName = "jpeg";

    /** height of the generated image
     */
    private int height;
    /** width of the generated image
     */
    private int width;

    private String rtl = "";
    private final static String cRTLstr = " -PRTL";
    public void setRTL( boolean on ) {
        if (on)
           rtl = cRTLstr;
        else
           rtl = "";
    }
    public boolean getRTL() {
       return( rtl.equals(cRTLstr) );
    }

    private boolean bTextAlpha = true;
    private String textAlphaOptStr = " -dTextAlphaBits=2";
    public boolean getTextAlpha() {
        return bTextAlpha;
    }
    public void setTextAlpha( boolean textAlpha ) {
        bTextAlpha = textAlpha;
        if ( bTextAlpha )
            textAlphaOptStr = " ";
        else
            textAlphaOptStr =  " -dTextAlphaBits=2";
    }

    private String deviceOptions =  " ";

    /** GhostPrinter application path */
    private String ghostAppStr;

    /** look for relative path executable first */
    //private String ghostAppRelStr =  "../../language_switch/obj/pspcl6";
    private String ghostAppRelStr =  "../../main/obj/pcl6";

    /** look for executable in path next */
    //private String ghostAppPathStr =  "pspcl6";
    private String ghostAppPathStr =  "pcl6";

    /** find the GhostPrinter application
     * first relative path
     * then system path
     * adds .exe for windows executables
     */
    private void setGhostApp() {
        ghostAppStr = ghostAppRelStr; 
        File testIt = new File(ghostAppStr);
        if (!testIt.exists()) {
            ghostAppStr = ghostAppRelStr + ".exe";
            testIt = new File(ghostAppStr);
            if (!testIt.exists()) {
                ghostAppStr = ghostAppPathStr;
                testIt = new File(ghostAppStr);
                if (!testIt.exists()) {
                    ghostAppStr = ghostAppPathStr + ".exe";
                    testIt = new File(ghostAppStr);
                    if (!testIt.exists()) {
                        System.out.println("Missing file " + ghostAppStr);
                        System.exit(1);
                    }
                }
            }
        }
    }

    /**
     * "command line" used to run interpreter.
     * NB Client should be able to modify settings.
     */
    private String runString( int page )
    {
        return ghostAppStr
            + deviceOptions
            + rtl
            + textAlphaOptStr
            + " -dFirstPage=" + page + " -dLastPage="
            + page
            + " -r" + xRes + "x" + yRes
            + " -sDEVICE=" + deviceName
            + " -dNOPAUSE "
            + " -sOutputFile=- " + jobList + " \n";
    }

    /**
     * "command line" to get the page count quickly
     */
    private String pageCountString()
    {
        return ghostAppStr
            + " -C -r25 -dNOPAUSE -sDEVICE=nullpage "
            + jobList;
    }

    /**
    * run the pcl interpreter to get a total page count for the job.
    * Returns -1 if there is an error.
    */
    public int getPrinterPageCount()
    {
        Process p;

        myTotalPageCount = -1;
	if ( debug ) System.out.println("Kill PageCount" + myTotalPageCount );
        try {
            if ( debug ) System.out.println("pagecountstring=" + pageCountString());
            p = Runtime.getRuntime().exec(pageCountString());
            String line;
            BufferedReader br = new BufferedReader(new InputStreamReader(p.getErrorStream()));
            while ((line = br.readLine()) != null)
                if ( line.startsWith("%%%") ) {
                    // place after cursor after ':'
                    int index = line.indexOf(':') + 1;
                    // convert to integer
                    myTotalPageCount = Integer.parseInt((line.substring(index)).trim());
                    if (debug) System.out.println(myTotalPageCount);
                    // NB error checking ??
                }
        } catch (Exception e) {
            System.out.println(e);
        }
	if ( debug ) System.out.println("Set PageCount" + myTotalPageCount );
        return myTotalPageCount;
    }

    // public methods.

    public void setDeviceOptions( String options ) {
       deviceOptions = options;
    }

    /** size of page cache */
    private final static int maxCached = 10;

    /** cache of page images, circular queue. */ 
    private BufferedImage[] pageCache;

    /** hash of the options send to generate the page should be unique enough */
    private int[] pageCacheHashCodes;

    /** page number */
    private int[] pageCacheNumber;

    /** Next cache location to store into, circular queue. */
    private int nextCache = 0;

    /**
    * run the pcl interpreter with the current settings and return a
    * buffered image for the page.  Sets height and width as a side
    * effect.
    */
    public BufferedImage getPrinterOutputPage( int page ) {
	int hit = -1;
	int i;

	for (i = 0; i < maxCached; i++ ) {
	    // search cache
	    if ( pageCacheNumber[i] == page ) {
		hit = i;
	    }
	}
	int hashCode = runString( page ).hashCode();
	BufferedImage bi = null;
	if ( hit >= 0 ) {
	    bi = pageCache[hit];
	    if ( bi != null && hashCode == pageCacheHashCodes[hit]) {
		// return cache hit 
		if (debug) System.out.println("used cached page :" + page + " at "+ hit);
		return bi;
	    }
	    // else right page but wrong file,resolution...
	}
	
        bi = getPrinterOutputPageNoCache( page );
        if ( bi == null )
            return null;
	
	// store in cache
        pageCache[nextCache] = bi;
	pageCacheNumber[nextCache] = page;
	pageCacheHashCodes[nextCache] = hashCode;
	if (debug) System.out.println("cached page :" + page + " at "+ nextCache);
	nextCache = (++nextCache) % maxCached;

        return bi;
    }

    /**
    * run the pcl interpreter with the current settings and return a
    * buffered image for the page.  Sets height and width as a side
    * effect.
    */

    public BufferedImage getPrinterOutputPageNoCache( int page )
    {
        int ch;
        int i = 0;
        Process p = null;

        try {
            if (debug) System.out.println("runstring=" + runString( page ));
            p = Runtime.getRuntime().exec(runString( page ));
            // read process output and return a buffered image.
            JPEGImageDecoder decoder = JPEGCodec.createJPEGDecoder(p.getInputStream());
            BufferedImage bim = decoder.decodeAsBufferedImage();
            this.height = bim.getHeight();
            this.width = bim.getWidth();
            return bim;
        } catch (Exception e) {
            // exception but we have a real process - read for errors
            if ( p != null ) {
                BufferedReader br = new BufferedReader(new InputStreamReader(p.getErrorStream()));
                String line;
                try {
                    while ((line = br.readLine()) != null)
                        System.out.println(line);
                } catch (IOException ie) {
                    System.out.println(e);
                }
            } else {
                System.out.println(e);
            }
            return null;
        }
    }

    /** A list of file names that comprise a job.
     * NB should be an array of String.
     */
    public void setJob(String jobList) {
        if (debug) {
	    System.out.println( "Set Job" );
	    System.out.println( pageCache );
	}
        this.jobList = jobList;
    }

    public String getJob() {
        return jobList;
    }

    /** NB needs error handling.
     * set x and y resolution.
     * @param Positive between 5 and 5000
     */
    public void setRes(double xRes, double yRes)
    {
        this.xRes = xRes;
        this.yRes = yRes;
    }

    public double getResX() {
        return xRes;
    }
    public double getResY() {
        return yRes;
    }

    /**
     * Page number to display.
     * Previous pages are "skipped".
     */
    public void setPageNumber(int page)
    {
        this.pageToDisplay = page;
    }
    public int getPageNumber() 
    {
	return pageToDisplay;
    }

    /** Accessor for image height.
     */
    public int getImgHeight()
    {
        return height;
    }
    /** Accessor for image width.
     */
    public int getImgWidth()
    {
        return width;
    }

    public Gpickle()
    {
        pageCache = new BufferedImage[maxCached];
        pageCacheHashCodes = new int [maxCached];
	pageCacheNumber = new int [maxCached];
        setGhostApp();
    }

    /** main for test purposes */
    public static void main( String[] args ) {
        debug = true;
        Gpickle pcl = new Gpickle();
        pcl.setRes(72, 72);
        pcl.setJob("120pg.bin");
        System.out.println(pcl.getPrinterPageCount());
        System.out.println(pcl.getPrinterOutputPage( 5 ));
        System.out.println("Width = " + pcl.getImgWidth());
        System.out.println("Height = " + pcl.getImgWidth());
        Gpickle pxl = new Gpickle();
        pxl.setJob("frs96.pxl");
        System.out.println(pcl.getPrinterPageCount());
        System.out.println(pxl.getPrinterOutputPage( 5 ));
        System.out.println("Width = " + pxl.getImgWidth());
        System.out.println("Height = " + pxl.getImgWidth());

    }
}
