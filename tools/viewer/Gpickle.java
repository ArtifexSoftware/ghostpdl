import java.awt.*;
import java.awt.image.*;
import java.io.*;
import com.sun.image.codec.jpeg.*;
import java.lang.Process;
import java.lang.Runtime;

// Interface to pcl/pxl interpreter

public class Gpickle {
    // here are some defaults that might be overriden will probably be
    // overriden.
    private int xRes = 100;
    private int yRes = 100;
    private String jobList = "startpage.pcl";
    private int pageToDisplay = 1;

    // we always use jpeg for output because java has good internal
    // support for jpeg.
    private String deviceName = "jpeg";

    // height and width of the generated image
    private int height;
    private int width;
    // private methods...
    private String runString()
    {
	return "pcl6 " +  " -dFirstPage=" + pageToDisplay + " -dLastPage=" + pageToDisplay + " -r" +
	    xRes + "x" + yRes + " -sDEVICE=" + deviceName + " -dNOPAUSE " + " -sOutputFile=- " + jobList + "\n";
    }

    // public methods.

    // run the pcl interpreter with the current settings and return a
    // buffered image for the page.  Sets height and width as a side
    // effect.

    public BufferedImage getPrinterOutputPage()
    {
        int ch;
	try {
	    Process p = Runtime.getRuntime().exec(runString());

	    // read process output and return a buffered image.
	    JPEGImageDecoder decoder = JPEGCodec.createJPEGDecoder(p.getInputStream());
	    BufferedImage bim = decoder.decodeAsBufferedImage();
	    this.height = bim.getHeight();
	    this.width = bim.getWidth();
	    return bim;
	} catch (Exception e) {
	    System.out.println(e);
	    return null;
	}
	    
    }

    // a list of file names that comprise a job. NB should be an array
    // of String.
    public void setJob(String jobList) {
	this.jobList = jobList;
    }

    // NB needs error handling.
    // set x and y resolution.
    public void setRes(int xRes, int yRes)
    {
	this.xRes = xRes;
	this.yRes = yRes;
    }

    public void setPageNumber(int page) 
    {
	this.pageToDisplay = page;
    }

    // accessors for height and width.
    public int getHeight()
    {
	return height;
    }
	
    public int getWidth()
    {
	return width;
    }

    // test
    public static void main( String[] args ) {
	Gpickle pcl = new Gpickle();
	pcl.setRes(72, 72);
	pcl.setPageNumber(5);
	pcl.setJob("120pg.bin");
	System.out.println(pcl.getPrinterOutputPage());
	System.out.println("Width = " + pcl.getWidth());
	System.out.println("Height = " + pcl.getWidth());
	Gpickle pxl = new Gpickle();
	pxl.setJob("frs96.pxl");
	System.out.println(pxl.getPrinterOutputPage());
	System.out.println("Width = " + pxl.getWidth());
	System.out.println("Height = " + pxl.getWidth());

    }
}
