import java.io.*;
import java.lang.Process;
import java.lang.Runtime;

// Interface to pcl/pxl interpreter

public class Gpickle {
    // here are some defaults that might be overriden will probably be
    // overriden.
    private int xRes = 100;
    private int yRes = 100;
    private int bitsPerComponent = 1;
    private int numComponents = 4;
    private String jobList = "startpage.pcl";
    private String deviceName = "bitcmyk";
    private String personality = "PCL5E";
    private String commandString = "pcl6 " + "-r" + xRes + "x" + yRes +
	" -sDEVICE=" + deviceName + " -P" + personality +
	" -dNOPAUSE " + " -sOutputFile=- " + jobList + "\n";
	
    // private methods...

    // NB problem - pcl can set the page size, resolution etc... thus
    // changing the size of the framebuffer... temporarily hardwired
    // to letter paper and resolution on entry.
    private int widthBytes()
    {
	int pixels = (int)((float)8.5 * xRes);
	return (pixels * bitsPerComponent * numComponents) / 8;
    }

    // hardwired to 11.0.
    private int height() 
    {
	return (int)((float)11.0 * yRes);
    }

    // size of framebuffer.
    private int frameSizeBytes() 
    {
	return widthBytes() * height();
    }

    // debug dump framebuffer
    private void dumpFrame(byte framebuf[])
    {
	int i;
	for (i = 0; i < framebuf.length; i++) {
	    if ( (i % 30) == 0 ) {
		System.out.println();
	    } else {
		System.out.print(framebuf[i] + " ");
	    }
	}
    }
	
    // public methods.

    // run the pcl interpreter with the current settings.
    public int runPrinter() {
	int ch;
	try {
	    // command string should contain all the command line
	    // options - see commandString field above.
	    Process p = Runtime.getRuntime().exec(commandString);
	    // get the size the framebuffer.
	    int bytes_wanted = frameSizeBytes();
	    byte[] buffer = new byte[bytes_wanted];

	    // read process output and store it in the framebuffer.
	    BufferedInputStream in = new BufferedInputStream(p.getInputStream());
	    int offset = 0;
	    int bytes_read;
	    while ((bytes_read = 
		    in.read(buffer, offset, buffer.length - offset)) > 0) {
		offset += bytes_read;
	    }

	    // woops - shouldn't happen.
	    if ( bytes_wanted != offset ) {
		System.out.println( "fatal error needed " +
				    bytes_wanted +
				    " bytes but only got " + 
				    offset + " bytes" );
	    } else {
		// decimal dump the buffer - here we should do something
		// like pass the data to the 2d api.
		dumpFrame(buffer);
	    }
	    p.destroy();
	} catch (IOException ioex) {
	    ioex.printStackTrace();
	}
	return 0;
    }

    
    // a list of file names that comprise a job.
    public int setJob(String jobList) {
	this.jobList = jobList;
	return 0;
    }
    // get a page count for the job.
    public int pageCount() {
	return -1;
    }

    // NB needs error handling.
    // set x and y resolution.
    public int setRes(int xRes, int yRes) {
	this.xRes = xRes;
	this.yRes = yRes;
	return 0;
    }

    // set bits per component.
    public int setBpc(int bpc) {
	this.bitsPerComponent = bpc;
	return 0;
    }

	
    // set number of components.
    public int setNumComp(int numComp) {
	this.numComponents = numComp;
	return 0;
    }
	
    // test
    public static void main( String[] args ) {
	Gpickle p = new Gpickle();
	p.runPrinter();
    }

}
