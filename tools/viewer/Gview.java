import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.awt.image.*;
import java.io.File;
import javax.swing.filechooser.*;
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

    protected final static boolean debug = false;
    protected BufferedImage currentPage;
    protected int pageNumber = 1;
    protected GpickleThread pickle;
    protected double desiredRes;
    protected double origRes;
    protected double origH;
    protected double origW;
    protected double origX = 0;
    protected double origY = 0;
    protected int tx;
    protected int ty;
    private boolean drag = false;

    // constructor
    public Gview()
    {
	super( "Ghost Pickle Viewer" );
	pageNumber = 1;
	pickle = new GpickleThread(this);
	//pickleThread = new GpickleThread(pickle, this);
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
	if ( key == KeyEvent.VK_PAGE_DOWN ) {
	    nextPage();
	    return;
	}
	else if ( key == KeyEvent.VK_PAGE_UP ) {
	    prevPage();
	    return;
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
        else if ( key == KeyEvent.VK_O ) {
	    runFileOpen();
	}
	else
           return;

	pickle.startProduction( pageNumber );

    }

     /** file open */ 
    void runFileOpen() {
	JFileChooser chooser = new JFileChooser();
	int result = chooser.showOpenDialog(null);
	File file = chooser.getSelectedFile();
	if (result == JFileChooser.APPROVE_OPTION) {
	    if (debug) System.out.println("file open " + file.getPath());

	    pickle.setJob(file.getPath());
	    pageNumber = 1;
	    pickle.startProduction( pageNumber );
	}
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


	if ( (e.getModifiers() & (e.BUTTON2_MASK | e.BUTTON3_MASK)) != 0 ) {
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

    public void setPage(int _pageNumber) {
       pageNumber = _pageNumber;
    }
    public void nextPage() {
	++pageNumber;					
        pickle.startProduction( pageNumber );	
    }
    public void prevPage() {
        --pageNumber;
        if ( pageNumber < 1 )
	    pageNumber = 1;
        pickle.startProduction( pageNumber );
    }

    protected void translate(int x, int y) {
        double x1 = origX = origX + x;
        double y1 = origY = origY + y;

        double sfx = desiredRes / origRes;
        double sfy = desiredRes / origRes;

        createViewPort( x1, y1, sfx, sfy, origRes, origRes);
    }
    protected void translateTo( double x1, double y1 ) {
        origX = x1;
        origY = y1;

        double sfx = desiredRes / origRes;
        double sfy = desiredRes / origRes;

        createViewPort( x1, y1, sfx, sfy, origRes, origRes);
   }

    protected void zoomIn( int x, int y ) {

        double x1 = origX = origX + (x * desiredRes / origRes);
        double y1 = origY = origY + (y * desiredRes / origRes);
	
        desiredRes *= 2.0f;

        double sfx = desiredRes / origRes;
        double sfy = desiredRes / origRes;

        createViewPort( x1, y1, sfx, sfy, origRes, origRes);
    }

    protected void zoomOut( int x, int y ) {
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
        pickle.startProduction( pageNumber );
    }

    /** main program */
    public static void main( String[] args )
    {
	// if (debug) 
	    System.out.print(usage());
	Gview view = new Gview();
        view.runMain(args);
    }

    /**
     * usage:
     */
    public static String usage() {
	String str =
	    "Usage::java Gview file.pcl\n"
	    + "q -> quit\n"
	    + "z -> zoomin\n"
	    + "x -> zoomout\n"
	    + "o -> open file\n"
	    + "PageUp & PageDown\n"
	    + "drag mouse1 -> translate\n"
	    + "mouse2 -> zoom in\n"
	    ;
	return str;
    }

    protected void runMain(String[] args) {
	// NB no error checking.
	pickle.setJob(args[0]);
	origRes = desiredRes = 100;
	pickle.setRes(desiredRes, desiredRes);
	pickle.setPageNumber(pageNumber);
	currentPage = pickle.getPrinterOutputPage();
	setSize(pickle.getImgWidth(), pickle.getImgHeight());
	origW = pickle.getImgWidth();
	origH = pickle.getImgHeight();
	show();
    }
}
