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
 * @version $Revision: 1749 $
*/

public class Gpickle {
    /** debug printf control */
    private static boolean debug = true;

    /** Render resolution */
    private double xRes = 75f;
    private double yRes = 75f;

    /** translate origin */
    private double xTrans = 0.0;    
    private double yTrans = 0.0;

    /** scalefactor */
    private double xScale = 1.0;
    private double yScale = 1.0;

    /** RTL PS command override */
    private double plotsize1 = 1.0;
    private double plotsize2 = 1.0;

    /** Tray orientation */
    private int trayOrientation = 0;

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

    private boolean bRtl = false;
    private final static String cRTLstr = "-PRTL";

    /** set -PRTL mode we do not force other modes to allow language switching
     * it maybe desireable to select mono/color 
     */
    public void setRTL( boolean on ) {
	bRtl = on;
    }
    public boolean getRTL() {
       return( bRtl );
    }

    private boolean bTextAlpha = true;
    private String textAlphaOptStr = "-dTextAlphaBits=2";
    public boolean getTextAlpha() {
        return bTextAlpha;
    }
    public void setTextAlpha( boolean textAlpha ) {
        bTextAlpha = textAlpha;
    }

    /** GhostPrinter application path */
    private String ghostAppStr;


    /** langSwitch add postscript using pspcl6 */
    private int langSwitch = 0;

    /** look for relative path to executable first 
     */
    private String[] ghostAppPathStr =  { 
	"../../main/obj/pcl6", 
	"../../language_switch/obj/pspcl6",
	"pcl6", 
	"pspcl6" 
    };

    /** look for executable in path next */

    /** find the GhostPrinter application
     * first relative path
     * then local directory 
     * default adds .exe for windows executables in search path
     */
    public void setGhostApp() {
        ghostAppStr = ghostAppPathStr[0 + langSwitch]; 
        File testIt = new File(ghostAppStr);
        if (!testIt.exists()) {
            ghostAppStr = ghostAppPathStr[0 + langSwitch] + ".exe";
            testIt = new File(ghostAppStr);
            if (!testIt.exists()) {
		// looking for current directory application.
                ghostAppStr = ghostAppPathStr[2 + langSwitch];
                testIt = new File(ghostAppStr);
                if (!testIt.exists()) {
                    ghostAppStr = ghostAppPathStr[2 + langSwitch] + ".exe";
		    // defaults to pcl6.exe, windows requires extension foo
		    // NB really should do a platform test on extension adding.
                }
            }
        }
    }

    /**
     * "command line" used to run interpreter.
     * NB Client should be able to modify settings.
     */
    private String[] runString( int page )
    {
	int i = 0;
	int numCommands = 15;
	if ( bTextAlpha )
	    ++numCommands;
	if (bRtl)
	    numCommands += 5;
        String[] commandArr = new String[numCommands];
        commandArr[i++] = ghostAppStr;
	if ( bTextAlpha ) {
	    commandArr[i++] = textAlphaOptStr;
	}
        commandArr[i++] = "-dViewTransX= "+ Double.toString(xTrans);
        commandArr[i++] = "-dViewTransY= "+ Double.toString(yTrans);
        commandArr[i++] = "-dViewScaleX= "+ Double.toString(xScale);
        commandArr[i++] = "-dViewScaleY= "+ Double.toString(yScale);
        commandArr[i++] = "-dFirstPage=" + Integer.toString(page); 
        commandArr[i++] = "-dLastPage=" + Integer.toString(page);
        commandArr[i++] = "-r" + Double.toString(xRes) + "x" + Double.toString(yRes);
        commandArr[i++] = "-sDEVICE=" + deviceName;
        commandArr[i++] = "-dNOPAUSE";
	commandArr[i++] = "-J@PJL SET USECIECOLOR=ON";
	commandArr[i++] = "-J@PJL SET VIEWER=ON";
	commandArr[i++] = "-dTrayOrientation=" + Integer.toString(trayOrientation);
	if (bRtl) {
	    commandArr[i++] = cRTLstr;
	    commandArr[i++] = "-J@PJL SET PLOTSIZEOVERRIDE=OFF";
	    commandArr[i++] = "-J@PJL SET PLOTSIZE1"+ Double.toString(plotsize1);	
	    commandArr[i++] = "-J@PJL SET PLOTSIZE2"+ Double.toString(plotsize2);
	    commandArr[i++] = "-J@PJL SET PLOTSIZEROTATE=OFF";
	}
        commandArr[i++] = "-sOutputFile=-";
        commandArr[i++] = jobList;   
	if (debug) {
	    for (int j = 0; j < commandArr.length; j++)
		System.out.println( commandArr[j] );
	}
        return commandArr;
    }

    /**
     * "command line" to get the page count quickly
     */
    private String pageCountString()
    {
        return ghostAppStr
            + " -C -r10 -dNOPAUSE -sDEVICE=nullpage "
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


    /** translation and scale options */
    public void setTranslateScaleOptions( double tx, double ty, double sx, double sy ) {
	xTrans = tx;
	yTrans = ty;
	xScale = sx;
	yScale = sy;
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
	// hash all strings together; only called from here.
	int hashCode = 0;
	String[] cmdStr = runString( page );
	for (i = 0; i<cmdStr.length; i++)
	    hashCode += cmdStr[i].hashCode();

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
    /** Accessor of TrayOrientation */
    public int getTrayOrientation()
    {
	return trayOrientation;
    }
    public void setTrayOrientation(int angle)
    {
	if ( angle == 0 ||
	     angle == 90 ||
	     angle == 180 ||
	     angle == 270 )
	    trayOrientation = angle;
    }
    public boolean getLangSwitch()
    {
	return( langSwitch == 1 );
    }
    public void setLangSwitch(boolean useLangSwitch)
    {
	if (useLangSwitch)
	    langSwitch = 1;
	else
	    langSwitch = 0;
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
