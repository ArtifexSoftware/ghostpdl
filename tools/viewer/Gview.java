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
 * @author Henry Stiles
 */
public class Gview extends JFrame implements KeyListener, /* MouseListener, */ GpickleObserver {

    private BufferedImage currentPage;
    private int pageNumber = 1;
    private Gpickle pickle = new Gpickle();
    private GpickleThread pickleThread;
    // constructor
    public Gview()
    {
	super( "Ghost Pickle Viewer" );
	pageNumber = 1;
	pickle = new Gpickle();
	pickleThread = new GpickleThread(pickle, this);
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

    /**
     * Usage:
     */
    public static void main( String[] args )
    {
	Gview view = new Gview();
	
	view.runArgs( args );
     }

     public void runArgs( String[] args )
     {
	// NB no error checking.
	pickle.setJob(args[0]);
	pickle.setRes(110,110);
	pickle.setPageNumber(pageNumber);
	currentPage = pickle.getPrinterOutputPage();
	setSize(pickle.getWidth(), pickle.getHeight());
	show();
    }
}
