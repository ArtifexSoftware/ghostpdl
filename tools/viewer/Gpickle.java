import java.awt.*;
import java.awt.image.*;
import java.io.*;
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
    private int pageToDisplay = 1;
    private int totalPageCount = -1;  // defaults to error

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

    private boolean bTextAlpha = false;
    private String textAlphaOptStr = " ";
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

    /**
     * "command line" used to run interpreter.
     * NB Client should be able to modify settings.
     */
    private String runString()
    {
	return "pcl6"
	    + deviceOptions
	    + rtl
	    + textAlphaOptStr
	    + " -dFirstPage=" + pageToDisplay + " -dLastPage="
            + pageToDisplay
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
	return "pcl6 -C -r25 -dNOPAUSE -sDEVICE=nullpage "
            + jobList;
    }

    /**
    * run the pcl interpreter to get a total page count for the job.
    * Returns -1 if there is an error.
    */
    public int getPrinterPageCount()
    {
        Process p;
        // if the page count is non-negative 0 it has already been set
        // for this job so just return the cached value.
        if ( this.totalPageCount >= 0 ) {
            if ( debug ) System.out.println("cached page count= " + this.totalPageCount);
            return this.totalPageCount;
        }

        this.totalPageCount = -1;
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
                    this.totalPageCount = Integer.parseInt((line.substring(index)).trim());
                    System.out.println(this.totalPageCount);
                    // NB error checking ??
                }
        } catch (Exception e) {
            System.out.println(e);
        }
        return this.totalPageCount;
    }

    // public methods.

    public void setDeviceOptions( String options ) {
       deviceOptions = options;
    }

    /**
    * run the pcl interpreter with the current settings and return a
    * buffered image for the page.  Sets height and width as a side
    * effect.
    */

    public BufferedImage getPrinterOutputPage()
    {
        int ch;
	int i = 0;
        Process p = null;     

	try {
	    if ( debug ) System.out.println("runstring=" + runString());
	    p = Runtime.getRuntime().exec(runString());
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
        this.totalPageCount = -1;
	this.jobList = jobList;
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

    /** main for test purposes */
    public static void main( String[] args ) {
        debug = true;
	Gpickle pcl = new Gpickle();
	pcl.setRes(72, 72);
	pcl.setPageNumber(5);
	pcl.setJob("120pg.bin");
        System.out.println(pcl.getPrinterPageCount());
	System.out.println(pcl.getPrinterOutputPage());
	System.out.println("Width = " + pcl.getImgWidth());
	System.out.println("Height = " + pcl.getImgWidth());
	Gpickle pxl = new Gpickle();
	pxl.setJob("frs96.pxl");
        System.out.println(pcl.getPrinterPageCount());
	System.out.println(pxl.getPrinterOutputPage());
	System.out.println("Width = " + pxl.getImgWidth());
	System.out.println("Height = " + pxl.getImgWidth());

    }
}
