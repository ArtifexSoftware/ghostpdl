import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.awt.image.*;

/**
 * Simple Viewer for PCL and PXL files.
 * PageUp and PageDown move between pages of a document.
 * Usage:
 * java Gview ../frs96.pxl
 * @version $Revision$
 * @author Henry Stiles 
 */
public class Gview extends JFrame implements KeyListener {

    // NB why static ?  I wouldn't mind two Frames in one application.
    private static BufferedImage currentPage;
    private static int pageNumber = 1;
    private static Gpickle page = new Gpickle();
    // constructor
    public Gview()
    {
	super( "Ghost Pickle Viewer" );
	addKeyListener(this);
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
	if ( key == 34 )
	    ++pageNumber;
	// page up
	else if ( key == 33 ) {
	    --pageNumber;
	    if ( pageNumber < 1 )
		pageNumber = 1;
	}
	page.setPageNumber(pageNumber);
	currentPage = page.getPrinterOutputPage();
	if ( currentPage != null )
	    repaint();
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

    /**
     * Usage:
     */
    public static void main( String[] args )
    {
	Gview view = new Gview();
	// NB no error checking.
	page.setJob(args[0]);
	page.setRes(75,75);
	page.setPageNumber(pageNumber);
	currentPage = page.getPrinterOutputPage();
	view.setSize(page.getWidth(), page.getHeight());
	view.show();
    }
}
