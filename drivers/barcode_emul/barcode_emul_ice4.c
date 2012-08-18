/*
 * driver/barcode_emul Barcode emulator driver
 *
 * Copyright (C) 2012 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/earlysuspend.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>
#include "barcode_emul_ice4.h"

struct barcode_emul_data {
	struct i2c_client		*client;
};

/*
 * Send barcode emulator firmware data thougth spi communication
 */
static int barcode_send_firmware_data(void)
{
	unsigned int i,j;	
	unsigned char spibit;

	i=0;
	while (i < CONFIGURATION_SIZE) {
		j=0;
		spibit=spiword[i];
		while (j < 8) {
			gpio_set_value(GPIO_FPGA_SPI_CLK,GPIO_LEVEL_LOW);

			if (spibit & 0x80) 
			{
				gpio_set_value(GPIO_FPGA_SPI_SI,GPIO_LEVEL_HIGH);
			}
			else
			{
				gpio_set_value(GPIO_FPGA_SPI_SI,GPIO_LEVEL_LOW);
			}
			
			j=j+1;
			gpio_set_value(GPIO_FPGA_SPI_CLK,GPIO_LEVEL_HIGH);
			spibit = spibit<<1;
		}
		i=i+1;
	}

	i=0;
	while (i<100) {
		gpio_set_value(GPIO_FPGA_SPI_CLK,GPIO_LEVEL_LOW);
		i=i+1;            
		gpio_set_value(GPIO_FPGA_SPI_CLK,GPIO_LEVEL_HIGH);
	}    
	return 0;
}

static int check_fpga_cdone(void)
{
    /* Device in Operation when CDONE='1'; Device Failed when CDONE='0'. */

    if (gpio_get_value(GPIO_FPGA_CDONE)!=1)
    	{
    		printk(KERN_ERR"CDONE_FAIL\n");
    	}
    else
    	{
    		printk(KERN_INFO"CDONE_SUCCESS\n");
    	}
/*
	printk(KERN_INFO"CDONE_STATE : %d\n",gpio_get_value(GPIO_FPGA_CDONE));
	while(!gpio_get_value(GPIO_FPGA_CDONE));
*/

/*
#if 0	
	gpio_set_value(GPIO_FPGA_RST_N,GPIO_LEVEL_HIGH);
		
    	gpio_set_value(GPIO_FPGA_RST_N,GPIO_LEVEL_LOW);
    	gpio_set_value(GPIO_FPGA_RST_N,GPIO_LEVEL_HIGH);
		
       gpio_set_value(GPIO_FPGA_RST_N,GPIO_LEVEL_LOW);
    	gpio_set_value(GPIO_FPGA_RST_N,GPIO_LEVEL_HIGH);
		
       gpio_set_value(GPIO_FPGA_RST_N,GPIO_LEVEL_LOW);
    	gpio_set_value(GPIO_FPGA_RST_N,GPIO_LEVEL_HIGH);
		
	gpio_set_value(GPIO_FPGA_RST_N,GPIO_LEVEL_LOW);
    	gpio_set_value(GPIO_FPGA_RST_N,GPIO_LEVEL_HIGH);
#else
	udelay(1000);
	gpio_set_value(GPIO_FPGA_SPI_EN,GPIO_LEVEL_HIGH);
	udelay(1);
	gpio_set_value(GPIO_FPGA_SPI_EN,GPIO_LEVEL_LOW);
	udelay(1);
	gpio_set_value(GPIO_FPGA_SPI_EN,GPIO_LEVEL_HIGH);
	udelay(1);
	gpio_set_value(GPIO_FPGA_SPI_EN,GPIO_LEVEL_LOW);
	udelay(1);
	gpio_set_value(GPIO_FPGA_SPI_EN,GPIO_LEVEL_HIGH);
	udelay(1);
	gpio_set_value(GPIO_FPGA_SPI_EN,GPIO_LEVEL_LOW);
	udelay(1);
	gpio_set_value(GPIO_FPGA_SPI_EN,GPIO_LEVEL_HIGH);
#endif
*/
    	return 0;
}

int barcode_fpga_fimrware_update_start(void)
{
	gpio_request_one(GPIO_FPGA_CDONE, GPIOF_IN, "FPGA_CDONE");
	gpio_request_one(GPIO_FPGA_SPI_CLK, GPIOF_OUT_INIT_LOW, "FPGA_SPI_CLK");
	gpio_request_one(GPIO_FPGA_SPI_SI, GPIOF_OUT_INIT_LOW, "FPGA_SPI_SI");
	gpio_request_one(GPIO_FPGA_SPI_EN, GPIOF_OUT_INIT_LOW, "FPGA_SPI_EN");
	gpio_request_one(GPIO_FPGA_CRESET_B, GPIOF_OUT_INIT_HIGH, "FPGA_CRESET_B");
	gpio_request_one(GPIO_FPGA_RST_N, GPIOF_OUT_INIT_HIGH, "FPGA_RST_N");

	gpio_set_value(GPIO_FPGA_CRESET_B,GPIO_LEVEL_LOW);
	udelay(10);
	    
	gpio_set_value(GPIO_FPGA_CRESET_B,GPIO_LEVEL_HIGH);
	udelay(400);

	barcode_send_firmware_data();
	udelay(50);

	check_fpga_cdone();    

	return 0;
}

void barcode_fpga_firmware_update(void)
{
	printk(KERN_INFO"=== barcode fpga start ===\n");
	barcode_fpga_fimrware_update_start();
}

static ssize_t barcode_emul_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	int ret, i;
	struct barcode_emul_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	struct {
		unsigned char addr;
		unsigned char data[20];
	} i2c_block_transfer;

	unsigned char barcode_data[14]={0xFF, 0xAC, 0xDB, 0x36, 0x42, 0x85, 0x0A, 0xA8, 0xD1, 0xA3, 0x46, 0xC5, 0xDA, 0xFF};

	if(gpio_get_value(GPIO_FPGA_CDONE)!=1)
	{
		printk(KERN_ERR "%s: cdone fail !!\n", __func__);
		return 0;
	}
#if 1

	ret = i2c_master_send(client, buf, size);
	if (ret < 0) {
		dev_err(&client->dev, "%s: err1 %d\n", __func__, ret);
		ret = i2c_master_send(client, buf, size);
		if (ret < 0)
			dev_err(&client->dev, "%s: err2 %d\n", __func__, ret);
	}
/*
	for (i = 0; i < size; i++)
		i2c_block_transfer.data[i] = *buf++;

	i2c_block_transfer.addr = 0x01;
	
	printk(KERN_INFO "%s: write addr: %d, value: %d\n", __func__, i2c_block_transfer.addr, i2c_block_transfer.data[0]);
	
	ret = i2c_master_send(client, (unsigned char *) &i2c_block_transfer, 2);
	if (ret < 0) {
		dev_err(&client->dev, "%s: err1 %d\n", __func__, ret);
		ret = i2c_master_send(client, (unsigned char *) &i2c_block_transfer, 2);
		if (ret < 0)
			dev_err(&client->dev, "%s: err2 %d\n", __func__, ret);
	}

	do{	
		unsigned char testbuf[10];
		struct i2c_msg msg[2];
		unsigned char addr=0x01;
		
		msg[0].addr  = client->addr;
		msg[0].flags = 0x00;
		msg[0].len   = 1;
		msg[0].buf   = &addr;

		msg[1].addr  = client->addr;
		msg[1].flags = I2C_M_RD;
		msg[1].len   = 1;
		msg[1].buf   = testbuf;

		ret=i2c_transfer(client->adapter, msg, 2); 
		if  (ret == 2) {
			printk(KERN_INFO "read value: %d\n", testbuf[0]);
		}
		else
			dev_err(&client->dev, "%s: err1 %d\n", __func__, ret);
	}while(0);
*/
#else
	i2c_block_transfer.addr = 0x00;
	i2c_block_transfer.data[0]=0x00;
	ret = i2c_master_send(client, (unsigned char *) &i2c_block_transfer, 2);
	if (ret < 0) {
		dev_err(&client->dev, "%s: err1 %d\n", __func__, ret);
		ret = i2c_master_send(client, (unsigned char *) &i2c_block_transfer, 2);
		if (ret < 0)
			dev_err(&client->dev, "%s: err2 %d\n", __func__, ret);
	}

	i2c_block_transfer.addr = 0x01;
	i2c_block_transfer.data[0]=0x01;
	ret = i2c_master_send(client, (unsigned char *) &i2c_block_transfer, 2);
	if (ret < 0) {
		dev_err(&client->dev, "%s: err1 %d\n", __func__, ret);
		ret = i2c_master_send(client, (unsigned char *) &i2c_block_transfer, 2);
		if (ret < 0)
			dev_err(&client->dev, "%s: err2 %d\n", __func__, ret);
	}

	i2c_block_transfer.addr = 0x02;
	for (i = 0; i < 14; i++)
		i2c_block_transfer.data[i] = barcode_data[i];
	
	ret = i2c_master_send(client, (unsigned char *) &i2c_block_transfer, 15);
	if (ret < 0) {
		dev_err(&client->dev, "%s: err1 %d\n", __func__, ret);
		ret = i2c_master_send(client, (unsigned char *) &i2c_block_transfer, 15);
		if (ret < 0)
			dev_err(&client->dev, "%s: err2 %d\n", __func__, ret);
	}

	i2c_block_transfer.addr = 0x00;
	i2c_block_transfer.data[0]=0x01;
	ret = i2c_master_send(client, (unsigned char *) &i2c_block_transfer, 2);
	if (ret < 0) {
		dev_err(&client->dev, "%s: err1 %d\n", __func__, ret);
		ret = i2c_master_send(client, (unsigned char *) &i2c_block_transfer, 2);
		if (ret < 0)
			dev_err(&client->dev, "%s: err2 %d\n", __func__, ret);
	}
#endif	
	return size;
}

static ssize_t barcode_emul_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return strlen(buf);
}
static DEVICE_ATTR(barcode_send, 0664, barcode_emul_show, barcode_emul_store);

static int __devinit barcode_emul_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct barcode_emul_data *data;
	struct device *barcode_emul_dev;
	int error;

	printk(KERN_INFO "%s probe!\n", __func__);
	//printk(KERN_INFO "name: %s,  addr: %d\n", client->name, client->addr);
	//show_stack(current, NULL);

	if(gpio_get_value(GPIO_FPGA_CDONE)!=1)
	{
		dev_err(&client->dev, "%s: cdone fail !!\n", __func__);
		return 0;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -EIO;

	data = kzalloc(sizeof(struct barcode_emul_data), GFP_KERNEL);
	if (NULL == data) {
		pr_err("Failed to data allocate %s\n", __func__);
		error = -ENOMEM;
		goto err_free_mem;
	}
	data->client = client;
	i2c_set_clientdata(client, data);
	
	barcode_emul_dev = device_create(sec_class, NULL, 0, data, "sec_barcode_emul");

	if (IS_ERR(barcode_emul_dev))
		pr_err("Failed to create barcode_emul_dev device\n");

	if (device_create_file(barcode_emul_dev, &dev_attr_barcode_send) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_barcode_send.attr.name);
	return 0;

err_free_mem:
	kfree(data);
	return error;
}

static int __devexit barcode_emul_remove(struct i2c_client *client)
{
	struct barcode_emul_data *data = i2c_get_clientdata(client);

	i2c_set_clientdata(client, NULL);
	kfree(data);
	return 0;
}

static const struct i2c_device_id ice4_id[] = {
	{"ice4", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, ice4_id);

static struct i2c_driver ice4_i2c_driver = {
	.driver = {
		.name = "ice4",
	},
	.probe = barcode_emul_probe,
	.remove = __devexit_p(barcode_emul_remove),
	.id_table = ice4_id,
};

static int __init barcode_emul_init(void)
{
	return i2c_add_driver(&ice4_i2c_driver);
}
module_init(barcode_emul_init);

static void __exit barcode_emul_exit(void)
{
	i2c_del_driver(&ice4_i2c_driver);
}
module_exit(barcode_emul_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SEC Barcode emulator");
