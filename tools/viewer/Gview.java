/* Portions Copyright (C) 2001 Artifex Software Inc.
   
   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

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
public class Gview 
    extends JFrame 
    implements KeyListener, MouseListener, MouseMotionListener, GpickleObserver 
{

    /** enables printfs */
    protected final static boolean debug = false;

    /** starting resolution 
     *  75dpi scales nicely to 300,600,1200
     *  100dpi gives a bigger starting window.
     *  50 or even 25 may be needed for a very large plot on a small memory machine.
     */
    protected double startingRes = 100;

    /** derived classes like: Nav will be 1/zoomWindowRatio in size
     * startingRes = 100; zoomWindowRation = 2; 
     * Nav window res is 50.
     */
    protected double zoomWindowRatio = 3;


    /** enable RTL mode in menu
     *  try with low startingRes = 25
     */
    protected boolean enableRTL = false;


    // Non configurable members below 

    protected int pageNumber = 1;
    protected int totalPageCount = 1;
    protected BufferedImage currentPage;
    protected GpickleThread pickle;
    protected double desiredRes;
    protected double origRes;
    protected double origH;
    protected double origW;
    protected double origX = 0;
    protected double origY = 0;
    protected int tx;
    protected int ty;
    protected boolean drag = false;

    /** File open, starts off in users home directory */
    private JFileChooser chooser = new JFileChooser();

    // lots of menu vars 
    private final static boolean popupMenuAllowed = true;
    private java.awt.PopupMenu      popup;
    private java.awt.Menu             menuFile;
    private java.awt.MenuItem           menuFileOpen;
    private java.awt.MenuItem           menuFileQuit;
    private java.awt.Menu             menuOpt;
    private java.awt.Menu               menuOptRes;
    private java.awt.MenuItem             menuOptResMinus;
    private java.awt.MenuItem             menuOptResPlus;
    private java.awt.CheckboxMenuItem   menuOptTextAntiAlias;
    private java.awt.CheckboxMenuItem   menuOptRTLMode;
    private java.awt.MenuItem         menuDPI;
    private java.awt.MenuItem         menuZoomIn;
    private java.awt.MenuItem         menuZoom600;
    private java.awt.MenuItem         menuZoomOut;
    private java.awt.MenuItem         menuPageNum;
    private java.awt.MenuItem         menuPageDwn;
    private java.awt.MenuItem         menuPageUp;

  
    /** constructor */
    public Gview()
    {
	super( "Ghost Pickle Viewer" );
	pageNumber = 1;
	pickle = new GpickleThread(this);

	addKeyListener(this);
	addMouseListener(this);
	addMouseMotionListener(this);

	initPopupMenu();     
	
	addWindowListener(new java.awt.event.WindowAdapter() {
		public void windowClosing(java.awt.event.WindowEvent evt) {
		    exitForm(evt);
		}
	    }
			  );
	
    }

    /** initialize popup menu */
    private void initPopupMenu() { 
	
	popup = new PopupMenu();
	
	menuFile = new java.awt.Menu();
	menuFile.setLabel("File");
	menuFileOpen = new java.awt.MenuItem();
	menuFileOpen.setLabel("Open");
	menuFileOpen.addActionListener(new java.awt.event.ActionListener() {
		public void actionPerformed(java.awt.event.ActionEvent evt) {
		    fileOpen();
		}
	    }
				       );
	menuFile.add(menuFileOpen);

        menuFileQuit = new java.awt.MenuItem();
	menuFileQuit.setLabel("Quit");
	menuFileQuit.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
                    System.exit (0);
                }
            }
				       );
	menuFile.add(menuFileQuit);

	popup.add(menuFile);

	menuOpt = new java.awt.Menu();
	menuOpt.setLabel("Options");

	menuOptRes = new java.awt.Menu();
	menuOptRes.setLabel("Res: " + startingRes);
        
	menuOptResMinus = new java.awt.MenuItem();
	menuOptResMinus.setLabel("Decrease Resolution");	
	menuOptResMinus.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    if (startingRes > 25)
			startingRes -= 10; 
		    menuOptRes.setLabel("Res: " + startingRes);
                }
            }
				  );
	menuOptRes.add(menuOptResMinus);

	menuOptResPlus = new java.awt.MenuItem();
	menuOptResPlus.setLabel("Increase Resolution");	
	menuOptResPlus.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    if (startingRes < 300)
			startingRes += 10; 
		    menuOptRes.setLabel("Res: " + startingRes);
                }
            }
				  );
	menuOptRes.add(menuOptResPlus);

	menuOpt.add(menuOptRes);

	menuOptTextAntiAlias = new java.awt.CheckboxMenuItem();
	
	menuOptTextAntiAlias.setState(pickle.getTextAlpha());
	menuOptTextAntiAlias.setLabel("TextAntiAlias");
        menuOptTextAntiAlias.addItemListener(new java.awt.event.ItemListener() {
		public void itemStateChanged(java.awt.event.ItemEvent evt) {
		    pickle.setTextAlpha( 
					!menuOptTextAntiAlias.getState() );
		}
	    }
				       );
	menuOpt.add(menuOptTextAntiAlias);

	if ( enableRTL ) {
	    menuOptRTLMode = new java.awt.CheckboxMenuItem();
	
	    menuOptRTLMode.setState(pickle.getRTL());
	    menuOptRTLMode.setLabel("RTL");
	    menuOptRTLMode.addItemListener(new java.awt.event.ItemListener() {
		    public void itemStateChanged(java.awt.event.ItemEvent evt) {
			// toggle rtl mode state 
			pickle.setRTL( ! pickle.getRTL() );
			// menuOptRTLMode.setEnabled(pickle.getRTL());
		    }
		}
					   );
	    menuOpt.add(menuOptRTLMode);
	}
	popup.add(menuOpt);

	popup.addSeparator();

        menuDPI = new java.awt.MenuItem();
	menuDPI.setLabel("dpi: " + desiredRes);
	menuDPI.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    menuDPI.setLabel("dpi: " + desiredRes);
                }
            }
				  );
	popup.add(menuDPI);

        menuZoomIn = new java.awt.MenuItem();
	menuZoomIn.setLabel("Zoom IN");
	menuZoomIn.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    origX = 0;
		    origY = 0;
		    zoomIn(0,0);     
		    menuDPI.setLabel("dpi: " + desiredRes);
                }
            }
				     );
	popup.add(menuZoomIn);

        menuZoom600 = new java.awt.MenuItem();
	menuZoom600.setLabel("Zoom to 600dpi");
	menuZoom600.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    zoomToRes(600.0f);
		    menuDPI.setLabel("dpi: " + desiredRes);
                }
            }
				     );
	popup.add(menuZoom600);

        menuZoomOut = new java.awt.MenuItem();
	menuZoomOut.setLabel("Zoom Out");
	menuZoomOut.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    origX = 0;
		    origY = 0;
		    zoomOut(0,0);    
		    menuDPI.setLabel("dpi: " + desiredRes);
                }
            }
				     );
	popup.add(menuZoomOut);

	popup.addSeparator();

        menuPageNum = new java.awt.MenuItem();
	menuPageNum.setLabel("page# " + pageNumber + " of " + totalPageCount);
	menuPageNum.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    menuPageNum.setLabel("page# " + pageNumber + " of " + totalPageCount);
                }
            }
				  );
	popup.add(menuPageNum);

        menuPageDwn = new java.awt.MenuItem();
	menuPageDwn.setLabel("Prev. Page");
	menuPageDwn.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    prevPage();
		    menuPageNum.setLabel("page# " + pageNumber + " of " + totalPageCount);
                }
            }
				     );
	popup.add(menuPageDwn);
    
	menuPageUp = new java.awt.MenuItem();
	menuPageUp.setLabel("Next Page");
	menuPageUp.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    nextPage();
		    menuPageNum.setLabel("page# " + pageNumber + " of " + totalPageCount);
                }
            }
				     );
	popup.add(menuPageUp);

	add( popup );
    }

    /** Exit the Application */
    private void exitForm(java.awt.event.WindowEvent evt) {//GEN-FIRST:event_exitForm
        System.exit (0);
    }

    /**
     * Page Up and Page Down Keys
     * Increment and Decrement the current page respectively.
     * Currently does't prevent keys strokes while a page is being generated,  
     * this give poor feedback when a key is pressed repeatedly while a long job runs.
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
	    fileOpen();
	}
	else
           return;

	pickle.startProduction( pageNumber );
    }

     /** file open */
    void fileOpen() {
        int result = chooser.showOpenDialog(null);
        File file = chooser.getSelectedFile();
        if (result == JFileChooser.APPROVE_OPTION) {
            if (debug) System.out.println("file open " + file.getPath());
            String[] args = new String[1];
            args[0] = file.getPath();
            runMain(args);
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

    /** used to drag translations in realtime
     */
    protected int lastX;
    protected int lastY;
    /** used to drag translations in realtime
     */
    protected int newX = 0;
    protected int newY = 0;
    public void mouseDragged(MouseEvent e) {
	lastX = newX;
	lastY = newY;
	newX = e.getX() - tx + (int)origX;
	newY = e.getY() - ty + (int)origY;
	repaint();
    }
    public void mouseMoved(MouseEvent e) {
    }

    /** paint frame contents */
    public void paint( Graphics g )
    {
	if (drag == true) {
	    // ugly but gets the point across 
	    g.drawImage(currentPage, 
			newX - (int)origX, 
			newY - (int)origY, 
			this);
	}
	else {
	    g.drawImage(currentPage, 0, 0, this);
	}
    }	

    /** callback from PickleObserver occurs when Image is complete. */
    public void imageIsReady( BufferedImage newImage ) {
	currentPage = newImage;

	// update menu status 
	menuDPI.setLabel("dpi: " + desiredRes);
	menuPageNum.setLabel("page# " + pageNumber + " of " + totalPageCount);

	repaint();
    }

    /** starts drag translation, or popup menu
     */ 
    public void mousePressed(MouseEvent e) {


	if ( (e.getModifiers() & (e.BUTTON2_MASK | e.BUTTON3_MASK)) != 0 ) {

	    if ( popupMenuAllowed ) {
		// show popup Menu
		popup.show(this, e.getX(), e.getY());
		return;
	    }
	    else {
		// if menus are fixed then right mouse button zooms
		if ( e.isControlDown() ) {
		    zoomOut(e.getX(), e.getY());
		}
		else {
		    zoomIn(e.getX(), e.getY());
		}
	    }
	}
	else {
	    if ( e.isControlDown() ) {
		// translate to origin
	        desiredRes = origRes;
		origX = 0;
		origY = 0;
	        translate(0,0);
	    }
	    else {
		// drag translation BEGIN
	        tx = e.getX();
	        ty = e.getY();
	        drag = true;
		setCursor( new Cursor(Cursor.MOVE_CURSOR) );
	    }
	}
    }

    /** ends drag translation */
    public void mouseReleased(MouseEvent e) {
        if (e.isControlDown() == false && drag) {
           translate(tx - e.getX(), ty - e.getY());
           drag = false;
	   setCursor( Cursor.getDefaultCursor() );
	}
    }

    public void mouseClicked(MouseEvent e) {
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

    /** Increase resolution by factor of 2 */
    protected void zoomIn( int x, int y ) {

        double x1 = origX = origX + (x * desiredRes / origRes);
        double y1 = origY = origY + (y * desiredRes / origRes);
	
        desiredRes *= 2.0f;

        double sfx = desiredRes / origRes;
        double sfy = desiredRes / origRes;

        createViewPort( x1, y1, sfx, sfy, origRes, origRes);
    }

    /** decrease resolution by factor of 2 */
    protected void zoomOut( int x, int y ) {
        double x1 = origX = origX + (x * desiredRes / origRes);
        double y1 = origY = origY + (y * desiredRes / origRes);
	
        desiredRes /= 2.0f;

        double sfx = desiredRes / origRes;
        double sfy = desiredRes / origRes;

        createViewPort( x1, y1, sfx, sfy, origRes, origRes );
    }

    /** Set zoom resolution to asked for resolution at 0,0
     */
    protected void zoomToRes( float res ) {		   
	origX = 0;
	origY = 0;
	desiredRes = res / 2.0; 
	zoomIn(0,0);  // internal multiply by 2 gets back to asked for res    
    }


    /** Generate a new page with  translation, scale, resolution  */
    private void createViewPort( double tx,   double ty,
				 double sx,   double sy,
				 double resX, double resY)
    {
	if ( debug ) {
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
	if ( debug ) System.out.println( options );
        pickle.setRes( resX, resY );
        pickle.setDeviceOptions( options  );
        pickle.startProduction( pageNumber );
    }

    /** main program */
    public static void main( String[] args )
    {
	if (args.length < 1) {
	    System.out.println("Error: Missing input file\n" + usage());
	    System.exit(1);
	}
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
	    + "mouse2 -> Popup Menu\n"
	    ;
	return str;
    }

    /** defaults settings for runJob, override for different defaults.
     */
    public void runMain( String[] args ) {
	runJob(args, startingRes, true);
    }

    /** setting page count so that multiple views can share the same page count result
     */
    protected void setPageCount( int pageCount ) {
	totalPageCount = pageCount;
	menuPageNum.setLabel("page# " + pageNumber + " of " + totalPageCount);
    }

    /** Sets the job, opening/reopening the window based on resolution.
     *  @param getPageCount determines if the page count should be computed.  
     */
    protected void runJob(String[] args, double resolution, boolean getPageCount) {	
	pickle.setJob(args[0]);  
        origRes = desiredRes = resolution;
	pickle.setRes(desiredRes, desiredRes);

	if (getPageCount == true) {
	    // get the total page count for the job
	    setPageCount(pickle.getPrinterPageCount());
	}

	pageNumber = 1;
	pickle.setPageNumber(pageNumber);
	currentPage = pickle.getPrinterOutputPage();
	setSize(pickle.getImgWidth(), pickle.getImgHeight());
	origW = pickle.getImgWidth();
	origH = pickle.getImgHeight();    
	show();
	repaint();
    }
}
