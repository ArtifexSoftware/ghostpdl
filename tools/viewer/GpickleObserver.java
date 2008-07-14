
import java.awt.image.*;

/** Similar to ImageObserver but for BufferedImage
 * @author Stefan Kemper
 * @version $Revision: 1563 $
 */
public interface GpickleObserver {
    /** newImage contains the completed image */
    void imageIsReady( BufferedImage newImage );

    /** pageCount contains the number of pages in the document */
    void pageCountIsReady( int pageCount );
}
