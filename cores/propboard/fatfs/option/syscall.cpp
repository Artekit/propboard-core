/*------------------------------------------------------------------------*/
/* Sample code of OS dependent controls for FatFs                         */
/* (C)ChaN, 2014                                                          */
/*------------------------------------------------------------------------*/

#include <PropAudio.h>
#include <stddef.h>
#include <stm32f4xx.h>
#include <stdlib.h>
#include "../ff.h"

volatile uint8_t fs_free;

#if _FS_REENTRANT

bool fs_busy(void)
{
	return (fs_free == 0);
}

/*------------------------------------------------------------------------*/
/* Create a Synchronization Object										  */
/*------------------------------------------------------------------------*/
/* This function is called in f_mount() function to create a new
/  synchronization object, such as semaphore and mutex. When a 0 is returned,
/  the f_mount() function fails with FR_INT_ERR.
*/

int ff_cre_syncobj (	/* 1:Function succeeded, 0:Could not create the sync object */
	BYTE vol,			/* Corresponding volume (logical drive number) */
	_SYNC_t *sobj		/* Pointer to return the created sync object */
)
{
	(void)(vol);
	(void)(sobj);

	fs_free = 1;
	*sobj = &fs_free;
	return 1;
}

/*------------------------------------------------------------------------*/
/* Delete a Synchronization Object                                        */
/*------------------------------------------------------------------------*/
/* This function is called in f_mount() function to delete a synchronization
/  object that created with ff_cre_syncobj() function. When a 0 is returned,
/  the f_mount() function fails with FR_INT_ERR.
*/

int ff_del_syncobj (	/* 1:Function succeeded, 0:Could not delete due to any error */
	_SYNC_t sobj		/* Sync object tied to the logical drive to be deleted */
)
{
	(void)(sobj);
	return 1;
}



/*------------------------------------------------------------------------*/
/* Request Grant to Access the Volume                                     */
/*------------------------------------------------------------------------*/
/* This function is called on entering file functions to lock the volume.
/  When a 0 is returned, the file function fails with FR_TIMEOUT.
*/

int ff_req_grant (	/* 1:Got a grant to access the volume, 0:Could not get a grant */
	_SYNC_t sobj	/* Sync object to wait */
)
{
	(void)(sobj);

	while (fs_free == 0);
	fs_free = 0;
	return 1;
}



/*------------------------------------------------------------------------*/
/* Release Grant to Access the Volume                                     */
/*------------------------------------------------------------------------*/
/* This function is called on leaving file functions to unlock the volume.
*/

void ff_rel_grant (
	_SYNC_t sobj	/* Sync object to be signaled */
)
{
	(void)(sobj);

	__disable_irq();

	fs_free = 1;

	if (Audio.updatePending())
	{
		// Activate PendSV interrupt
		Activate_PendSV();
	}
	__enable_irq();
}

#endif


#if _USE_LFN == 3	/* LFN with a working buffer on the heap */
/*------------------------------------------------------------------------*/
/* Allocate a memory block                                                */
/*------------------------------------------------------------------------*/
/* If a NULL is returned, the file function fails with FR_NOT_ENOUGH_CORE.
*/

void* ff_memalloc (	/* Returns pointer to the allocated memory block */
	UINT msize		/* Number of bytes to allocate */
)
{
	return malloc(msize);	/* Allocate a new memory block with POSIX API */
}


/*------------------------------------------------------------------------*/
/* Free a memory block                                                    */
/*------------------------------------------------------------------------*/

void ff_memfree (
	void* mblock	/* Pointer to the memory block to free */
)
{
	free(mblock);	/* Discard the memory block with POSIX API */
}

#endif
