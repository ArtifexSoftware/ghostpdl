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
import java.awt.print.*;
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
 * @version $Revision: 1749 $
 * @author Henry Stiles, Stefan Kemper
 */
public class Gview 
    extends JFrame 
    implements KeyListener, MouseListener, MouseMotionListener, GpickleObserver, Printable
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
    /** fetches images page at a time */
    protected GpickleThread pickle;
    /** counts the number of pages in document */
    protected GpickleThread pageCounter;  
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
    private java.awt.MenuItem           menuFilePrintPage;
    private java.awt.MenuItem           menuFileQuit;
    private java.awt.Menu             menuOpt;
    private java.awt.Menu               menuOptRes;
    private java.awt.MenuItem             menuOptResMinus;
    private java.awt.MenuItem             menuOptResPlus;
    private java.awt.CheckboxMenuItem   menuOptTextAntiAlias;
    private java.awt.CheckboxMenuItem   menuOptPSandPCL;
    private java.awt.CheckboxMenuItem   menuOptRTLMode;
    private java.awt.MenuItem         menuDPI;
    private java.awt.MenuItem         menuZoomIn;
    private java.awt.MenuItem         menuZoomOut;
    private java.awt.Menu             menuZoom;
    private java.awt.MenuItem           menuZoom1_8;
    private java.awt.MenuItem           menuZoom1_4;
    private java.awt.MenuItem           menuZoom1_2;
    private java.awt.MenuItem           menuZoom1_1;
    private java.awt.MenuItem           menuZoom2_1;
    private java.awt.MenuItem           menuZoom4_1;
    private java.awt.MenuItem           menuZoom8_1;
    private java.awt.Menu             menuZoomToDPI;
    private java.awt.MenuItem           menuZoom25;
    private java.awt.MenuItem           menuZoom72;
    private java.awt.MenuItem           menuZoom100;
    private java.awt.MenuItem           menuZoom150;
    private java.awt.MenuItem           menuZoom300;
    private java.awt.MenuItem           menuZoom600;
    private java.awt.MenuItem           menuZoom1200;
    private java.awt.MenuItem           menuZoom2400;
    private java.awt.MenuItem         menuPageNum;
    private java.awt.MenuItem         menuPageDwn;
    private java.awt.MenuItem         menuPageUp;

  
    /** constructor */
    public Gview()
    {
	super( "Ghost Pickle Viewer" );
	pageNumber = 1;
	pickle = new GpickleThread(this);
	pageCounter = new GpickleThread(this);

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

	menuFilePrintPage = new java.awt.MenuItem();
	menuFilePrintPage.setLabel("Print Page (alpha)");
	menuFilePrintPage.addActionListener(new java.awt.event.ActionListener() {
		public void actionPerformed(java.awt.event.ActionEvent evt) {
		    filePrintPage();
		}
	    }
				       );
	menuFile.add(menuFilePrintPage);

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
		    pickle.setTextAlpha( ! pickle.getTextAlpha() );
		}
	    }
				       );
	menuOpt.add(menuOptTextAntiAlias);

	menuOptPSandPCL = new java.awt.CheckboxMenuItem();
	
	menuOptPSandPCL.setState(pickle.getLangSwitch());
	menuOptPSandPCL.setLabel("Language Switching pspcl6");
        menuOptPSandPCL.addItemListener(new java.awt.event.ItemListener() {
		public void itemStateChanged(java.awt.event.ItemEvent evt) {
		    pickle.setLangSwitch( ! pickle.getLangSwitch() );
		    pickle.setGhostApp();
		}
	    }
				       );
	menuOpt.add(menuOptPSandPCL);




	if ( enableRTL ) {
	    menuOptRTLMode = new java.awt.CheckboxMenuItem();
	
	    menuOptRTLMode.setState(pickle.getRTL());
	    menuOptRTLMode.setLabel("RTL");
	    menuOptRTLMode.addItemListener(new java.awt.event.ItemListener() {
		    public void itemStateChanged(java.awt.event.ItemEvent evt) {
			// toggle rtl mode state 
			pickle.setRTL( ! pickle.getRTL() );
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
	menuZoomIn.setLabel("Zoom In");
	menuZoomIn.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    origX = 0;
		    origY = 0;
		    zoomIn(0,0);     
		    menuDPI.setLabel("dpi: " + desiredRes);
		    System.out.println("menuZoomIn");
                }
            }
				     );
	popup.add(menuZoomIn);

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

	menuZoom = new java.awt.Menu();
	menuZoom.setLabel("Zoom");
	
        menuZoom1_8 = new java.awt.MenuItem();
	menuZoom1_8.setLabel("1:8");
	menuZoom1_8.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    zoomFactor(1/8.0);
		    menuDPI.setLabel("dpi: " + desiredRes);
                }
            }
				     );
	menuZoom.add(menuZoom1_8);

        menuZoom1_4 = new java.awt.MenuItem();
	menuZoom1_4.setLabel("1:4");
	menuZoom1_4.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    zoomFactor(1/4.0);
		    menuDPI.setLabel("dpi: " + desiredRes);
                }
            }
				     );
	menuZoom.add(menuZoom1_4);

        menuZoom1_2 = new java.awt.MenuItem();
	menuZoom1_2.setLabel("1:2");
	menuZoom1_2.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    zoomFactor(1/2.0);
		    menuDPI.setLabel("dpi: " + desiredRes);
                }
            }
				     );
	menuZoom.add(menuZoom1_2);

        menuZoom1_1 = new java.awt.MenuItem();
	menuZoom1_1.setLabel("1:1");
	menuZoom1_1.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    zoomToRes(startingRes);
		    menuDPI.setLabel("dpi: " + desiredRes);
                }
            }
				     );
	menuZoom.add(menuZoom1_1);

        menuZoom2_1 = new java.awt.MenuItem();
	menuZoom2_1.setLabel("2:1");
	menuZoom2_1.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    zoomFactor(2.0);
		    menuDPI.setLabel("dpi: " + desiredRes);
                }
            }
				     );
	menuZoom.add(menuZoom2_1);

        menuZoom4_1 = new java.awt.MenuItem();
	menuZoom4_1.setLabel("4:1");
	menuZoom4_1.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    zoomFactor(4.0);
		    menuDPI.setLabel("dpi: " + desiredRes);
                }
            }
				     );
	menuZoom.add(menuZoom4_1);

        menuZoom8_1 = new java.awt.MenuItem();
	menuZoom8_1.setLabel("8:1");
	menuZoom8_1.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    zoomFactor(8.0);
		    menuDPI.setLabel("dpi: " + desiredRes);
                }
            }
				     );
	menuZoom.add(menuZoom8_1);

	popup.add(menuZoom);

	menuZoomToDPI = new java.awt.Menu();
	menuZoomToDPI.setLabel("Zoom DPI");

        menuZoom25 = new java.awt.MenuItem();
	menuZoom25.setLabel("25 dpi");
	menuZoom25.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    zoomToRes(25.0);
		    menuDPI.setLabel("dpi: " + desiredRes);
                }
            }
				     );
	menuZoomToDPI.add(menuZoom25);

        menuZoom72 = new java.awt.MenuItem();
	menuZoom72.setLabel("72 dpi");
	menuZoom72.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    zoomToRes(72.0);
		    menuDPI.setLabel("dpi: " + desiredRes);
                }
            }
				     );
	menuZoomToDPI.add(menuZoom72);

        menuZoom100 = new java.awt.MenuItem();
	menuZoom100.setLabel("100 dpi");
	menuZoom100.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    zoomToRes(100.0);
		    menuDPI.setLabel("dpi: " + desiredRes);
                }
            }
				     );
	menuZoomToDPI.add(menuZoom100);

        menuZoom150 = new java.awt.MenuItem();
	menuZoom150.setLabel("150 dpi");
	menuZoom150.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    zoomToRes(150.0);
		    menuDPI.setLabel("dpi: " + desiredRes);
                }
            }
				     );
	menuZoomToDPI.add(menuZoom150);

        menuZoom300 = new java.awt.MenuItem();
	menuZoom300.setLabel("300 dpi");
	menuZoom300.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    zoomToRes(300.0);
		    menuDPI.setLabel("dpi: " + desiredRes);
                }
            }
				     );
	menuZoomToDPI.add(menuZoom300);

        menuZoom600 = new java.awt.MenuItem();
	menuZoom600.setLabel("600 dpi");
	menuZoom600.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    zoomToRes(600.0);
		    menuDPI.setLabel("dpi: " + desiredRes);
                }
            }
				     );
	menuZoomToDPI.add(menuZoom600);

        menuZoom1200 = new java.awt.MenuItem();
	menuZoom1200.setLabel("1200 dpi");
	menuZoom1200.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    zoomToRes(1200.0);
		    menuDPI.setLabel("dpi: " + desiredRes);
                }
            }
				     );
	menuZoomToDPI.add(menuZoom1200);

        menuZoom2400 = new java.awt.MenuItem();
	menuZoom2400.setLabel("2400 dpi");
	menuZoom2400.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
		    zoomToRes(2400.0);
		    menuDPI.setLabel("dpi: " + desiredRes);
                }
            }
				     );
	menuZoomToDPI.add(menuZoom2400);

	popup.add(menuZoomToDPI);

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
	    return;
	}
	else if ( key == KeyEvent.VK_X ) {
            origX = 0;
            origY = 0;
	    zoomOut(0,0);
	    return;
	}
        else if ( key == KeyEvent.VK_R ) {
	    // hack a smaller resolution and zoom ratio for rtl
	    // missing undo.
	    startingRes = 50;
	    zoomWindowRatio = 4;
            pickle.setRTL(!pickle.getRTL());  // toggle
        }
        else if ( key == KeyEvent.VK_T ) {
	    pickle.setTrayOrientation((pickle.getTrayOrientation() + 90 ) % 360);
	}
        else if ( key == KeyEvent.VK_O ) {
	    fileOpen();
	    return;
	}
        else if ( key == KeyEvent.VK_M ) {
	    popup.show(this, 50, 50);
	    return;
	}
	else { // default keystroke behaviour, brings up menu
	   popup.show(this, 50, 50);
           return;
	}
	pickle.startProduction( pageNumber );
    }

    /** file open dialog 
     */
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

    /** Prints the current page, not the Job
     */
    void filePrintPage() {
        PrinterJob job = PrinterJob.getPrinterJob();
        PageFormat pf = job.pageDialog(job.defaultPage());
        job.setPrintable(this, pf);
        job.setJobName( "New Fangled Pickle Job Pg #" + pageNumber );
        if (job.printDialog()) {
            // Print the job if the user didn't cancel printing
            try { 
                job.print();
            }
            catch (Exception e) {
                System.out.println(e.getMessage());
                e.printStackTrace();
            }
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
	    // setSize is the frame, not the contentpane :(
	    // g.drawImage(currentPage, 5, 20, this);
	    g.drawImage(currentPage, 0, 0, this);
	}
    }	
    
    public int print( Graphics g, PageFormat pf, int PageIndex ) throws PrinterException
    {
        // just printing the current page.
        if ( PageIndex > 0 )
            return Printable.NO_SUCH_PAGE;
        Graphics2D g2 = (Graphics2D) g;
        g2.drawImage(currentPage, 0, 0, this);
        return Printable.PAGE_EXISTS;
    }

    /** callback from PickleObserver occurs when Image is complete. */
    public void imageIsReady( BufferedImage newImage ) {
	currentPage = newImage;

	// update menu status 
	menuDPI.setLabel("dpi: " + desiredRes);
	setPageCount( totalPageCount );
	repaint();
    }

    /** count of pages done, update status display */
    public void pageCountIsReady( int pageCount ) {
	setPageCount( pageCount );
    }

    /** starts drag translation, or popup menu
     */ 
    public void mousePressed(MouseEvent e) {
	if (debug) 
	    System.out.println("mousePressed " + e + 
			       " B1 " + (e.getModifiers() & (e.BUTTON1_MASK)) + 
			       " B2 " + (e.getModifiers() & (e.BUTTON2_MASK)) + 
			       " B3 " + (e.getModifiers() & (e.BUTTON3_MASK)) + 
			       " Ctrl " + e.isControlDown() 
			       );
	// search for mouse button presses.
	if ( (e.getModifiers() & e.BUTTON1_MASK) != 0 ) {	
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
	else if ( (e.getModifiers() & (e.BUTTON2_MASK | e.BUTTON3_MASK)) != 0 ) {
	    if ( !popupMenuAllowed ) {
		// if menus are fixed then right mouse button zooms
		if ( e.isControlDown() ) {
		    zoomOut(e.getX(), e.getY());
		}
		else {
		    zoomIn(e.getX(), e.getY());
		}
	    }    
	    else {
		return;
		// show menu on release // popup.show(this, e.getX(), e.getY());
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
	else if ( (e.getModifiers() & (e.BUTTON2_MASK | e.BUTTON3_MASK)) != 0 ) {
	    if ( popupMenuAllowed ) 
		popup.show(this, e.getX(), e.getY());
	}
    }

    /** Avoid using this since mousePressed and mouseRelease are being used. */
    public void mouseClicked(MouseEvent e) {
    }
    public void mouseEntered(MouseEvent e) {
    }
    public void mouseExited(MouseEvent e) {
    }

    public void setPage(int _pageNumber) {
	if (totalPageCount > 0 && _pageNumber > totalPageCount)
	    // repair case where out of bounds page is requested before page count is known.
	    pageNumber = totalPageCount;
 	else
	    pageNumber = _pageNumber;
    }

    /** Render and display the next page,
     * stopping at the last page in document.
     * NOTE: it is possible to request beyond the last page if the request comes faster 
     * than the computation of the number of pages in the job.
     */
    public void nextPage() {
	if (totalPageCount > 0 && pageNumber >= totalPageCount)
	    // repair case where out of bounds page is requested before page count is known.
	    pageNumber = totalPageCount;
 	else
	    ++pageNumber;				   

        pickle.startProduction( pageNumber );	
    }

    /** Render and display the previous page */
    public void prevPage() {
	if (totalPageCount > 0 && pageNumber > totalPageCount) {
	    // repair case where out of bounds page is requested before page count is known.
	    pageNumber = totalPageCount;
	}
	else {
	    --pageNumber;
	    if ( pageNumber < 1 ) 
		pageNumber = 1;
	}
        pickle.startProduction( pageNumber );
    }

    /** translate a viewport, moves by an offset
     */ 
    protected void translate(int x, int y) {
        double x1 = origX = origX + x;
        double y1 = origY = origY + y;

        double sfx = desiredRes / origRes;
        double sfy = desiredRes / origRes;

        createViewPort( x1, y1, sfx, sfy, origRes, origRes);
    }

    /** translate a viewport, moves top,left of viewport to a coordinate 
     */
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
	if ( debug ) System.out.println( "zoomIn " + desiredRes );	
        desiredRes *= 2.0;
	if ( debug ) System.out.println( "zoomIn " + desiredRes );	


        double sfx = desiredRes / origRes;
        double sfy = desiredRes / origRes;

        createViewPort( x1, y1, sfx, sfy, origRes, origRes);
    }

    /** decrease resolution by factor of 2 */
    protected void zoomOut( int x, int y ) {

	double x1 = origX = origX + (x * desiredRes / origRes);
        double y1 = origY = origY + (y * desiredRes / origRes);
	
        desiredRes /= 2.0;

        double sfx = desiredRes / origRes;
        double sfy = desiredRes / origRes;

        createViewPort( x1, y1, sfx, sfy, origRes, origRes );
    }

    /** Set zoom resolution to asked for resolution at 0,0
     */
    protected void zoomToRes( double res ) {		
	if ( res < startingRes) {
	    desiredRes = res;
	    origRes = desiredRes;
	    createViewPort( 0.0, 0.0, 1.0, 1.0, origRes, origRes );
	}
	else if ( origRes < startingRes )
	{
	    origRes = startingRes;
	    origX = 0;
	    origY = 0;
	    desiredRes = res / 2.0;
	    zoomIn(0,0);  // internal multiply by 2 gets back to asked for res
	}
	else {
	    origX = 0;
	    origY = 0;
	    desiredRes = res / 2.0;
	    zoomIn(0,0);  // internal multiply by 2 gets back to asked for res
	}
	return;
    }

    /** Set zoom resolution to asked for resolution at 0,0
     */
    protected void zoomFactor( double factor ) {	
	zoomToRes(desiredRes*factor);
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
        pickle.setRes( resX, resY );
        pickle.setTranslateScaleOptions( tx, ty, sx, sy  );
	// disable lookahead for zoom and translate
        pickle.startProduction( pageNumber, false);
    }

    /** main program */
    public static void main( String[] args )
    {
	Gview view = new Gview();

	if (args.length < 1) {  
	    File file = new File("GhostPrinter.pcl");
	    if (file.exists()) {
		// open demo file
		args = new String[1];
		args[0] = new String("GhostPrinter.pcl");
	    }
	}
	System.out.print(usage());

	if (args.length < 1) {
	    // no demo file start with file open
	    view.fileOpen();
	}
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
     *  Note that the page count is also set in the pickleTreads
     */
    protected synchronized void setPageCount( int pageCount ) {
	totalPageCount = pageCount;
	if ( totalPageCount < 0 ) {
	    menuPageNum.setLabel("page# " + pageNumber + " of ?");
	    setTitle("GhostPickle: " +
		     pickle.getJob() +
		     " " +
		     pageNumber +
		     " of ?");
	}
	else {
	    menuPageNum.setLabel("page# " + pageNumber + " of " + totalPageCount);
	    setTitle("GhostPickle: " +
		     pickle.getJob() +
		     " " +
		     pageNumber +
		     " of " +
		     totalPageCount);
	}
	pickle.setPageCount(pageCount);
	pageCounter.setPageCount(pageCount);
    }

    /** Sets the job, opening/reopening the window based on resolution.
     *  @param getPageCount determines if the page count should be computed.
     */
    protected void runJob(String[] args, double resolution, boolean getPageCount) {	
	pickle.setJob(args[0]);
        origRes = desiredRes = resolution;
	pickle.setRes(desiredRes, desiredRes);

	// set the total page count as unknown for now
	if (getPageCount == true) {
	    setPageCount(-1);
	    // get the total page count for the job
	    pageCounter.setJob(args[0]);  
	    pageCounter.startCountingPages();
	}
	else
	    setPageCount(totalPageCount);

	pageNumber = 1;
	pickle.setPageNumber(pageNumber);
	currentPage = pickle.getPrinterOutputPage( pageNumber );
	setSize(pickle.getImgWidth(), pickle.getImgHeight());
	origW = pickle.getImgWidth();
	origH = pickle.getImgHeight();  	
	show();
	repaint();
    }
}
