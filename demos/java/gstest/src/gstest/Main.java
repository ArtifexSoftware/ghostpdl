package gstest;

import static com.artifex.gsjava.GSAPI.*;

import java.io.File;

import com.artifex.gsjava.GSAPI;
import com.artifex.gsjava.GSParam;
import com.artifex.gsjava.GSParams;
import com.artifex.gsjava.util.*;

public class Main {

	public static void main(String[] args) {
		LongReference iref = new LongReference();
		int code;
		if ((code = gsapi_new_instance(iref, GS_NULL)) != GS_ERROR_OK)
			throw new IllegalStateException("new instance returned " + code);

		long instance = iref.value;

		gsapi_set_arg_encoding(instance, 1);

		String[] gsargs = new String[] {
			"gs",
			"-sDEVICE=pngalpha",
			"-r100",
		};

		gsapi_init_with_args(instance, gsargs);

		Reference<Integer> aaRef = new Reference<>();
		if ((code = gsapi_get_param_once(instance, "TextAlphaBits", aaRef, GS_SPT_INT)) < 0) {
			throw new IllegalStateException("gsapi_get_param_once returned " + code);
		}
		System.out.println("TextAlpaBits=" + aaRef.getValue());

		ByteArrayReference resRef = new ByteArrayReference();
		if ((code = gsapi_get_param_once(instance, "HWResolution", resRef, GS_SPT_PARSED)) < 0) {
			throw new IllegalStateException("gsapi_get_param_once returned " + code);
		}
		System.out.println(resRef.asString());

		GSParams params = GSParams.getParams(instance);
		for (GSParam<?> param : params) {
			System.out.println(param);
		}

		gsapi_delete_instance(instance);
	}
}
