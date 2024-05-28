#ifndef __LIBQDMA_SHARED_API_H__
#define __LIBQDMA_SHARED_API_H__

/**
 * Queue direction types
 * @ingroup libqdma_enums
 *
 */
enum queue_type_t {
	/** host to card */
	Q_H2C,
	/** card to host */
	Q_C2H,
	/** cmpt queue*/
	Q_CMPT,
	/** Both H2C and C2H directions*/
	Q_H2C_C2H,
};

#endif
