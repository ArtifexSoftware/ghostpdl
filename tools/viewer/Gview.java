import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.awt.image.*;

/**
 * Simple Viewer for PCL and PXL files.
 * Use:
 *  Keyboard:
 *   PageUp and PageDown move between pages of a document.
 *   'q' quits
 *
 * Usage:
 * java Gview ../frs96.pxl
 *
 * @version $Revision$
 * @author Henry Stiles, Stefan Kemper
 */
public class Gview extends JFrame implements KeyListener, MouseListener,  GpickleObserver {

    final static private boolean debug = true; 
    private BufferedImage currentPage;
    private int pageNumber = 1;
    private Gpickle pickle = new Gpickle();
    private GpickleThread pickleThread;
    private double desiredRes;
    private double origRes;
    private double origH;
    private double origW;
    private double origX = 0;
    private double origY = 0;
    private int tx;
    private int ty;
    private boolean drag = false;

    // constructor
    public Gview()
    {
	super( "Ghost Pickle Viewer" );
	pageNumber = 1;
	pickle = new Gpickle();
	pickleThread = new GpickleThread(pickle, this);
	addKeyListener(this);
	addMouseListener(this);
    }

    /**
     * Page Up and Page Down Keys
     * Increment and Decrement the current page respectively.
     * Currently does't prevent keys strokes while a page is being generated,  this give poor feedback when a key is pressed repeatedly while a long job runs.
     */
    public void keyPressed( KeyEvent e )
    {
	int key = e.getKeyCode();
	// page down NB - increment past last page - BufferedImage
	// will be null and no repaint operation
	if ( key == KeyEvent.VK_PAGE_DOWN )
	    ++pageNumber;
	else if ( key == KeyEvent.VK_PAGE_UP ) {
	    --pageNumber;
	    if ( pageNumber < 1 )
		pageNumber = 1;
	}
	else if ( key == KeyEvent.VK_Q ) {
	    System.exit(1);
	}      
        else if ( key == KeyEvent.VK_Z ) {
            origX = 0;
            origY = 0;
	    zoomIn(0,0);
	}
	else if ( key == KeyEvent.VK_X ) {
            origX = 0;
            origY = 0;
	    zoomOut(0,0);
	}
        else if ( key == KeyEvent.VK_R ) {
            pickle.setRTL(!pickle.getRTL());  // toggle
        }
        else
           return;

	pickleThread.startProduction( pageNumber );

    }

    /**
     * Unused required by KeyListener
     */
    public void keyTyped( KeyEvent e )
    {
    }

    /**
     * Unused required by KeyListener
     */
    public void keyReleased( KeyEvent e )
    {
    }

    public void paint( Graphics g )
    {
	g.drawImage(currentPage, 0, 0, this);
    }	

    public void imageIsReady( BufferedImage newImage ) {
	currentPage = newImage;
	repaint();
    }

    public void mousePressed(MouseEvent e) {


	if ( (e.getModifiers() & e.BUTTON2_MASK) != 0 ) {
	    if ( e.isControlDown() ) {
		zoomOut(e.getX(), e.getY());
	    }
	    else {
		zoomIn(e.getX(), e.getY());
	    }
	}
	else {
	    if ( e.isControlDown() ) {
	        desiredRes = origRes;
		origX = 0;
		origY = 0;
	        translate(0,0);
	    }
	    else {
	        tx = e.getX();
	        ty = e.getY();
	        drag = true;
	    }
	}
	
	
    }
    public void mouseClicked(MouseEvent e) {
    }
    public void mouseReleased(MouseEvent e) {
        if (e.isControlDown() == false && drag) {
           translate(tx - e.getX(), ty - e.getY());
           drag = false;
	}     
    }
    public void mouseEntered(MouseEvent e) {
    }
    public void mouseExited(MouseEvent e) {
    }

    private void translate(int x, int y) {
        double x1 = origX = origX + x; 
        double y1 = origY = origY + y; 

        double sfx = desiredRes / origRes;
        double sfy = desiredRes / origRes;

        createViewPort( x1, y1, sfx, sfy, origRes, origRes);
    }

    private void zoomIn( int x, int y ) {

        double x1 = origX = origX + (x * desiredRes / origRes);
        double y1 = origY = origY + (y * desiredRes / origRes);
	
        desiredRes *= 2.0f;

        double sfx = desiredRes / origRes;
        double sfy = desiredRes / origRes;

        createViewPort( x1, y1, sfx, sfy, origRes, origRes);
    }

    private void zoomOut( int x, int y ) {
        double x1 = origX = origX + (x * desiredRes / origRes);
        double y1 = origY = origY + (y * desiredRes / origRes);
	
        desiredRes /= 2.0f;

        double sfx = desiredRes / origRes;
        double sfy = desiredRes / origRes;

        createViewPort( x1, y1, sfx, sfy, origRes, origRes );
    }

    /** Generate a new page with  translation, scale, resolution  */
    private void createViewPort( double tx,   double ty, 
				 double sx,   double sy, 
				 double resX, double resY)
    {
	if ( false && debug ) {
	    System.out.println( "viewTrans"
				+ "," + tx
				+ "," + ty
				);
	    System.out.println( "viewScale"
				+ "," + sx
				+ "," + sy
				);
	    System.out.println( "HWRes"
				+ " , " + resX
				+ " , " + resY
				);
	}

        String options =
	    " -dViewTransX="
	    + tx
	    + " -dViewTransY="
	    + ty
	    + " -dViewScaleX="
	    + sx
	    + " -dViewScaleY="
	    + sy;
	if ( false && debug) System.out.println( options );
        pickle.setRes( resX, resY );
        pickle.setDeviceOptions( options  );
        pickleThread.startProduction( pageNumber );
    }

    /** main program */
    public static void main( String[] args )
    {
	if (debug) System.out.print(usage());
	Gview view = new Gview();
        view.runMain(args);
    }

    /**
     * usage:
     */
    public static String usage() {
	String str =
	    "Usage::java Gview file.pcl\n"
	    + "q ->quit\n"
	    + "drag mouse1 -> translate\n"
	    + "crtl mouse1 -> reset zoom \n"
	    + "mouse2 -> zoom in\n"
	    ;
	return str;
    }

    private void runMain(String[] args) {
	// NB no error checking.
	pickle.setJob(args[0]);
	origRes = desiredRes = 75;
	pickle.setRes(desiredRes, desiredRes);
	pickle.setPageNumber(pageNumber);
	currentPage = pickle.getPrinterOutputPage();
	setSize(pickle.getImgWidth(), pickle.getImgHeight());
	origH = pickle.getImgWidth();
	origW = pickle.getImgHeight();
	show();
    }
}
