/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <chrono>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>

extern "C" {
#include "include/pd_driver.h"
#include "include/platform.h"
#include "ppm_common.h"
#include "smbus_usermode.h"
#include "um_ppm_chardev.h"
}

using namespace std;
using namespace testing;

class pdDriverMock {
    public:
	virtual ~pdDriverMock()
	{
	}

	/* mocked method(s) */
	MOCK_METHOD(int, execute_cmd,
		    (struct ucsi_pd_device *, struct ucsi_control *,
		     uint8_t *));
};

class TestFixture : public ::testing::Test {
    public:
	TestFixture()
	{
		_pdDriverMock.reset(new ::testing::StrictMock<pdDriverMock>());
	}

	~TestFixture()
	{
		_pdDriverMock.reset();
	}

	/* pointer for accessing mocked PD driver */
	static std::unique_ptr<pdDriverMock> _pdDriverMock;
};

/* instantiate mocked PD driver */
std::unique_ptr<pdDriverMock> TestFixture::_pdDriverMock;

/* wrapper to call mocked execute_cmd */
int execute_cmd(struct ucsi_pd_device *dev, struct ucsi_control *ctrl,
		uint8_t *lpm_data_out)
{
	return TestFixture::_pdDriverMock->execute_cmd(dev, ctrl, lpm_data_out);
}

class Cmd {
    public:
	Cmd(string name, vector<uint8_t> ref_cmd)
	{
		this->name = name;
		this->ref_cmd = ref_cmd;
	}

	Cmd(string name, vector<uint8_t> ref_cmd, vector<uint8_t> ref_resp)
	{
		this->name = name;
		this->ref_cmd = ref_cmd;
		this->ref_resp = ref_resp;
	}

	Cmd(string name, vector<uint8_t> ref_cmd, int ref_resp_val)
	{
		this->name = name;
		this->ref_cmd = ref_cmd;
		this->ref_resp_val = ref_resp_val;
	}

	typedef int (Cmd::*handle_execute_cmd_ptr)(struct ucsi_pd_device *,
						   struct ucsi_control *,
						   uint8_t *);

	int handle_execute_cmd(struct ucsi_pd_device *dev,
			       struct ucsi_control *ctrl, uint8_t *lpm_data_out)
	{
		uint8_t ref_cmd_bytes[UCSI_CMD_LEN] = { 0 };
		size_t rlen = ref_resp.size();
		size_t clen = ref_cmd.size();
		bool cmp;

		DLOG("RECEIVED COMMAND ITER(%d): command 0x%x, data_length 0x%x,"
		     "command_specific 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		     ++counter, ctrl->command, ctrl->data_length,
		     ctrl->command_specific[0], ctrl->command_specific[1],
		     ctrl->command_specific[2], ctrl->command_specific[3],
		     ctrl->command_specific[4], ctrl->command_specific[5]);

		memcpy(ref_cmd_bytes, ref_cmd.data(), clen);
		cmp = !memcmp(ctrl, ref_cmd_bytes, UCSI_CMD_LEN);
		if (!cmp) {
			DLOG_START("REFERENCE COMMAND: size 0x%x, data", clen);
			for (uint8_t val : ref_cmd)
				DLOG_LOOP(" 0x%x", val);
			DLOG_END("\n");
		}

		EXPECT_TRUE(cmp)
			<< "RCV COMMAND DOESN'T MATCH REF " << name << " CMD";

		DLOG("VALUE RESP_VAL=0x%x, RLEN=0x%x", ref_resp_val, rlen);

		if (rlen > 0)
			memcpy(lpm_data_out, ref_resp.data(), rlen);

		return ref_resp_val ? ref_resp_val : rlen;
	}

	static void reset_counter()
	{
		counter = 0;
	}

    private:
	string name;
	int ref_resp_val;
	vector<uint8_t> ref_cmd;
	vector<uint8_t> ref_resp;
	static atomic<int> counter;
	static const size_t UCSI_CMD_LEN = 8;
};

atomic<int> Cmd::counter{ 0 };

class Checker {
    public:
	static const int TMO_IN_MS = 5000;

	void addDirToCheck(string &dir, bool exist = true)
	{
		location.push_back(make_tuple(dir, exist, ""));
	}

	void addFileToCheck(string &file, string &value, bool exist = true)
	{
		location.push_back(make_tuple(file, exist, value));
	}

	int verify(int timeout_ms)
	{
		for (auto tuple : location) {
			auto loc = get<0>(tuple);
			auto exist = get<1>(tuple);
			auto val = get<2>(tuple);
			auto tmo = timeout_ms;
			bool file_exist;

			while (tmo) {
				ifstream file(loc);

				file_exist = file.good();
				if ((file_exist && exist) ||
				    (!file_exist && !exist)) {
					if (!val.length())
						break;

					string rval;
					getline(file, rval);
					if (!val.compare(rval))
						break;

					DLOG("Value '%s' in file '%s' does not match expected value '%s'",
					     rval.c_str(), loc.c_str(),
					     val.c_str());
					return -EINVAL;
				}

				this_thread::sleep_for(
					chrono::milliseconds(SLEEP_IN_MS));
				tmo -= SLEEP_IN_MS;
			}

			if (tmo <= 0) {
				DLOG("Failed to verify %s '%s' %s",
				     !val.length() ? "directory" : "file",
				     loc.c_str(),
				     exist ? "does exist" : "does not exist");
				return -ETIMEDOUT;
			}

			if (!val.length() || !exist) {
				DLOG("Verified '%s' %s %s", loc.c_str(),
				     !val.length() ? "directory" : "file",
				     exist ? "does exist" : "does not exist");
			} else
				DLOG("Verified file '%s' with value '%s' exists",
				     loc.c_str(), val.c_str());
		}

		return 0;
	}

	int verify()
	{
		return verify(TMO_IN_MS);
	};

	void clear()
	{
		location.clear();
	}

	vector<tuple<string, bool, string> > location;

	const size_t SLEEP_IN_MS = 250;
};

class OpmUnitTest : public TestFixture {
    public:
	OpmUnitTest()
		: num_ports(2)
	{
		/* initialize ucsi_pd_driver */
		pd_drv.dev = reinterpret_cast<struct ucsi_pd_device *>(this);
		pd_drv.configure_lpm_irq = configure_lpm_irq;
		pd_drv.init_ppm = init_ppm;
		pd_drv.get_ppm = get_ppm;
		pd_drv.execute_cmd = execute_cmd;
		pd_drv.get_active_port_count = pd_get_active_port_count;
		pd_drv.cleanup = pd_cleanup;

		/* initialize smbus_drv */
		smbus_drv.dev = reinterpret_cast<struct smbus_device *>(this);
		smbus_drv.block_for_interrupt = block_for_interrupt;
		smbus_drv.cleanup = smbus_cleanup;

		/* reset counter */
		Cmd::reset_counter();
	}

	virtual ~OpmUnitTest()
	{
	}

	void SetUp()
	{
		ppm_drv = ppm_open(&pd_drv);
		if (!ppm_drv)
			FAIL() << "Initializing ppm driver failed.";

		handle = platform_task_init((void *)main_loop, this);
		if (!handle)
			FAIL() << "Failed to start main loop.";
	}

	void TearDown()
	{
		/* clean up ppm */
		ppm_drv->cleanup(ppm_drv);

		/* clean up main loop (cdev) */
		platform_kill(handle, SIGTERM);
		platform_task_complete(handle);
	}

	static void main_loop(void *ptr)
	{
		OpmUnitTest *p = reinterpret_cast<OpmUnitTest *>(ptr);
		string sdev("/dev/ucsi_um_test-0");

		cdev_prepare_um_ppm(sdev.c_str(), p->get_pd_drv(),
				    p->get_smbus_drv(), p->get_config());
	}

	static int pd_get_active_port_count(struct ucsi_pd_device *dev)
	{
		return ppm_cast(dev)->_get_active_port_count();
	}

	static struct ucsi_ppm_driver *get_ppm(struct ucsi_pd_device *dev)
	{
		return ppm_cast(dev)->get_ppm_drv();
	}

	static int init_ppm(struct ucsi_pd_device *dev)
	{
		return ppm_cast(dev)->init_ppm_int();
	}

    protected:
	int init_ppm_int()
	{
		return ppm_drv->init_and_wait(ppm_drv->dev, 2);
	};

	struct ucsi_ppm_driver *get_ppm_drv()
	{
		return ppm_drv;
	};

	struct ucsi_pd_driver *get_pd_drv()
	{
		return &pd_drv;
	};

	struct smbus_driver *get_smbus_drv()
	{
		return &smbus_drv;
	};

	struct pd_driver_config *get_config()
	{
		return &config;
	};

	int _get_active_port_count()
	{
		return num_ports;
	};

	static OpmUnitTest *ppm_cast(struct ucsi_pd_device *dev)
	{
		return reinterpret_cast<OpmUnitTest *>(dev);
	}

	static int configure_lpm_irq(struct ucsi_pd_device *dev)
	{
		return 0;
	}

	static int block_for_interrupt(struct smbus_device *device)
	{
		while (1)
			this_thread::sleep_for(std::chrono::milliseconds(1000));
		return 0;
	}

	static void pd_cleanup(struct ucsi_pd_driver *driver)
	{
	}

	static void smbus_cleanup(struct smbus_driver *driver)
	{
	}

	void verify_port_properties(int port_num)
	{
		string num = to_string(port_num);
		Checker checker;

		string port_dir("/sys/class/typec/port" + num);
		checker.addDirToCheck(port_dir);

		string am0_svid_file(port_dir + "/port" + num + ".0/svid");
		string am0_svid_value("8087");
		checker.addFileToCheck(am0_svid_file, am0_svid_value);

		string am1_svid_file(port_dir + "/port" + num + ".1/svid");
		string am1_svid_value("17ef");
		checker.addFileToCheck(am1_svid_file, am1_svid_value);

		string am2_svid_file(port_dir + "/port" + num + ".2/svid");
		string am2_svid_value("ff01");
		checker.addFileToCheck(am2_svid_file, am2_svid_value);

		string pd_rev_file(port_dir + "/usb_power_delivery_revision");
		string pd_rev_value("3.0");
		checker.addFileToCheck(pd_rev_file, pd_rev_value);

		string typec_rev_file(port_dir + "/usb_typec_revision");
		string typec_rev_value("1.3");
		checker.addFileToCheck(typec_rev_file, typec_rev_value);

		EXPECT_EQ(0, checker.verify())
			<< "FAILED TO VERIFY PORT" << port_num << " PROPERTIES";
	}

	void verify_port_partner_properties(int port_num)
	{
		string num = to_string(port_num);
		Checker checker;

		string port_dir("/sys/class/typec/port" + num + "-partner");
		checker.addDirToCheck(port_dir);

		string am_desc_file(port_dir + "/port" + num +
				    "-partner.0/description");
		string am_desc_value("DisplayPort");
		checker.addFileToCheck(am_desc_file, am_desc_value);

		string am_svid_file(port_dir + "/port" + num +
				    "-partner.0/svid");
		string am_svid_value("ff01");
		checker.addFileToCheck(am_svid_file, am_svid_value);

		string am_num_file(port_dir + "/number_of_alternate_modes");
		string am_num_value("1");
		checker.addFileToCheck(am_num_file, am_num_value);

		string pd_rev_file(port_dir + "/usb_power_delivery_revision");
		string pd_rev_value("2.0");
		checker.addFileToCheck(pd_rev_file, pd_rev_value);

		EXPECT_EQ(0, checker.verify())
			<< "FAILED TO VERIFY PORT" << port_num
			<< "-PARTNER PROPERTIES";
	}

	struct pd_driver_config config = { .max_num_ports = 2,
					   .port_address_map = { 0x67, 0x66 } };
	struct ucsi_pd_driver pd_drv;
	struct ucsi_ppm_driver *ppm_drv;
	struct smbus_driver smbus_drv;
	struct um_ppm_cdev *cdev;
	struct task_handle *handle;
	int num_ports;
};

/*
 * UCSI 3.0 commands length is 8 bytes
 * The reference commands below do not include the trailing zeros
 * The commands were captured on the Realtek EVB with firmware 0.6.1
 */

/* PPM RESET command */
Cmd ppm_reset("ppm_reset", vector<uint8_t>({ 0x01 }));

/* SET_NCAM conn 1 command */
Cmd set_ncam_c1("set_ncam_c1", vector<uint8_t>({ 0xf, 0x0, 0x81, 0xff }));

/* SET_NCAM conn 2 command */
Cmd set_ncam_c2("set_ncam_c2", vector<uint8_t>({ 0xf, 0x0, 0x82, 0xff }));

/* SET_NOTIFICATION_ENABLE command */
Cmd set_notificaiton_en_1("set_notification_en_1",
			  vector<uint8_t>({ 0x5, 0x0, 0x1, 0x80 }));

/* ACK_CC_CI command - command completed ack */
Cmd ack_cc_ci("ack_cc_ci", vector<uint8_t>({ 0x4, 0x0, 0x2 }));

/* GET_CAPABILITY command */
Cmd get_caps("get_caps", vector<uint8_t>({ 0x6 }),
	     vector<uint8_t>({ 0x44, 0x1, 0x0, 0x0, 0x2, 0xb4, 0x0, 0x0, 0x3,
			       0x0, 0x20, 0x1, 0x0, 0x3, 0x30, 0x1 }));

/* GET_CONNECTOR_CAPABILITY conn 1 command */
Cmd get_conn_caps_c1("get_conn_caps_c1", vector<uint8_t>({ 0x7, 0x0, 0x1 }),
		     vector<uint8_t>({ 0xe4, 0x37, 0x0, 0x10 }));

/* GET_PDO conn 1 source command */
Cmd get_pdo_c1_src("get_pdo_c1_src",
		   vector<uint8_t>({ 0x10, 0x0, 0x01, 0x00, 0x07 }),
		   vector<uint8_t>({ 0x2c, 0x91, 0x11, 0x37 }));

/* GET_PDO conn 1 sink index 0 command */
Cmd get_pdo_c1_snk_i0("get_pdo_c1_snk_i0",
		      vector<uint8_t>({ 0x10, 0x0, 0x01, 0x00, 0x03 }),
		      vector<uint8_t>({ 0xa, 0x90, 0x1, 0x26, 0xc8, 0xd0, 0x2,
					0x0, 0xc8, 0xc0, 0x3, 0x0, 0xc8, 0xb0,
					0x4, 0x0 }));

/* GET_PDO conn 1 sink index 4 command */
Cmd get_pdo_c1_snk_i4("get_pdo_c1_snk_i4",
		      vector<uint8_t>({ 0x10, 0x0, 0x01, 0x04, 0x02 }),
		      vector<uint8_t>({ 0x2c, 0x41, 0x6, 0x0, 0xc8, 0x90, 0x41,
					0x9a }));

/* GET_ALTERNATE_MODE conn 1 index 0 command */
Cmd get_alt_mode_conn_c1_i0(
	"get_alt_mode_conn_c1_i0",
	vector<uint8_t>({ 0x0c, 0x00, 0x00, 0x01, 0x00, 0x00 }),
	vector<uint8_t>({ 0x87, 0x80, 0x0, 0x0, 0x0, 0x0 }));

/* GET_ALTERNATE_MODE conn 1 index 1 command */
Cmd get_alt_mode_conn_c1_i1(
	"get_alt_mode_conn_c1_i1",
	vector<uint8_t>({ 0x0c, 0x00, 0x00, 0x01, 0x01, 0x00 }),
	vector<uint8_t>({ 0xef, 0x17, 0x0, 0x0, 0x0, 0x0 }));

/* GET_ALTERNATE_MODE conn 1 index 2 command */
Cmd get_alt_mode_conn_c1_i2(
	"get_alt_mode_conn_c1_i2",
	vector<uint8_t>({ 0x0c, 0x00, 0x00, 0x01, 0x02, 0x00 }),
	vector<uint8_t>({ 0x1, 0xff, 0x46, 0x1c, 0x0, 0x40 }));

/* GET_CONNECTOR_STATUS conn 1 command */
Cmd get_conn_status_c1("get_conn_status_c1",
		       vector<uint8_t>({ 0x12, 0x00, 0x01 }),
		       vector<uint8_t>({ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
					 0x1, 0xc0, 0x0, 0x0, 0x0, 0x0, 0x0,
					 0x0, 0x0, 0x0, 0x0 }));

/* GET_CONNECTOR_CAPABILITY conn 2 command */
Cmd get_conn_caps_c2("get_conn_caps_c2", vector<uint8_t>({ 0x7, 0x00, 0x2 }),
		     vector<uint8_t>({ 0xe4, 0x37, 0x0, 0x10 }));

/* GET_PDO conn 2 source command */
Cmd get_pdo_c2_src("get_pdo_c2_src",
		   vector<uint8_t>({ 0x10, 0x00, 0x02, 0x00, 0x07 }),
		   vector<uint8_t>({ 0x2c, 0x91, 0x11, 0x37 }));

/* GET_PDO conn 2 sink index 0 command */
Cmd get_pdo_c2_snk_i0("get_pdo_c2_snk_i0",
		      vector<uint8_t>({ 0x10, 0x00, 0x02, 0x00, 0x03 }),
		      vector<uint8_t>({ 0xa, 0x90, 0x1, 0x26, 0xc8, 0xd0, 0x2,
					0x0, 0xc8, 0xc0, 0x3, 0x0, 0xc8, 0xb0,
					0x4, 0x0 }));

/* GET_PDO conn 2 sink index 4 command */
Cmd get_pdo_c2_snk_i4("get_pdo_c2_snk_i4",
		      vector<uint8_t>({ 0x10, 0x00, 0x02, 0x04, 0x02 }),
		      vector<uint8_t>({ 0x2c, 0x41, 0x6, 0x0, 0xc8, 0x90, 0x41,
					0x9a }));

/* GET_ALTERNATE_MODE conn 2 index 0 command */
Cmd get_alt_mode_c2_i0("get_alt_mode_c2_i0",
		       vector<uint8_t>({ 0x0c, 0x00, 0x00, 0x02, 0x00, 0x00 }),
		       vector<uint8_t>({ 0x87, 0x80, 0x0, 0x0, 0x0, 0x0 }));

/* GET_ALTERNATE_MODE conn 2 index 1 command */
Cmd get_alt_mode_c2_i1("get_alt_mode_c2_i1",
		       vector<uint8_t>({ 0x0c, 0x00, 0x00, 0x02, 0x01, 0x00 }),
		       vector<uint8_t>({ 0xef, 0x17, 0x0, 0x0, 0x0, 0x0 }));

/* GET_ALTERNATE_MODE conn 2 index 2 command */
Cmd get_alt_mode_c2_i2("get_alt_mode_c2_i2",
		       vector<uint8_t>({ 0x0c, 0x00, 0x00, 0x02, 0x02, 0x00 }),
		       vector<uint8_t>({ 0x1, 0xff, 0x46, 0x1c, 0x0, 0x40 }));

/* GET_CONNECTOR_STATUS conn 2 command */
Cmd get_conn_status_c2("get_conn_status_c2",
		       vector<uint8_t>({ 0x12, 0x00, 0x02 }),
		       vector<uint8_t>({ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
					 0x1, 0xc0, 0x0, 0x0, 0x0, 0x0, 0x0,
					 0x0, 0x0, 0x0, 0x0 }));

/* SET_NOTIFICATION_ENABLE command */
Cmd set_notificaiton_en_2("set_notification_en_2",
			  vector<uint8_t>({ 0x5, 0x0, 0xe7, 0xdb }));

/* Messages when LPM alert after connecting partner to connector 1 happens */

/* GET_CONNECTOR_STATUS conn 1 update 1 command */
Cmd get_conn_status_c1_update1(
	"get_conn_status_c1_update1", vector<uint8_t>({ 0x12, 0x00, 0x01 }),
	vector<uint8_t>({ 0x0, 0x40, 0x3d, 0x40, 0x0, 0x0, 0x0, 0x0, 0x8, 0xc0,
			  0x40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 }));

/* GET_CONNECTOR_STATUS conn 1 update 2 command */
Cmd get_conn_status_c1_update2("get_conn_status_c1_update2",
			       vector<uint8_t>({ 0x12, 0x00, 0x01 }),
			       vector<uint8_t>({ 0x60, 0x0, 0x3b, 0x40, 0x5a,
						 0x68, 0x1, 0x13, 0x8, 0xc0,
						 0x40, 0x0, 0x0, 0x0, 0x0, 0x0,
						 0x0, 0x0, 0x0 }));

/* GET_CONNECTOR_STATUS conn 1 update 3 command */
Cmd get_conn_status_c1_update3("get_conn_status_c1_update3",
			       vector<uint8_t>({ 0x12, 0x00, 0x01 }),
			       vector<uint8_t>({ 0x0, 0x10, 0x2b, 0x40, 0x5a,
						 0x68, 0x1, 0x13, 0x1, 0xc0,
						 0x40, 0x0, 0x0, 0x0, 0x0, 0x0,
						 0x0, 0x0, 0x0 }));

/* GET_CONNECTOR_STATUS conn 1 update 4 command */
Cmd get_conn_status_c1_update4("get_conn_status_c1_update4",
			       vector<uint8_t>({ 0x12, 0x00, 0x01 }),
			       vector<uint8_t>({ 0x60, 0x2, 0x2b, 0x40, 0x2c,
						 0xb1, 0x84, 0x43, 0x1, 0xc0,
						 0x40, 0x0, 0x0, 0x0, 0x0, 0x0,
						 0x0, 0x0, 0x0 }));

/* GET_CONNECTOR_STATUS conn 1 update 5 command */
Cmd get_conn_status_c1_update5("get_conn_status_p1_update5",
			       vector<uint8_t>({ 0x12, 0x00, 0x01 }),
			       vector<uint8_t>({ 0x0, 0x1, 0x4b, 0x40, 0x2c,
						 0xb1, 0x84, 0x43, 0x1, 0xc0,
						 0x40, 0x0, 0x0, 0x0, 0x0, 0x0,
						 0x0, 0x0, 0x0 }));

/* GET_PDOS conn 1 partner source error */
Cmd get_pdo_c1_partner_src_err("get_pdo_c1_partner_src_err",
			       vector<uint8_t>({ 0x10, 0x00, 0x81, 0x00,
						 0x07 }),
			       -1);

/* GET_ERROR_STATUS command */
Cmd get_error_status("get_error_status", vector<uint8_t>({ 0x13 }),
		     vector<uint8_t>({ 0x04, 0x0, 0x0, 0x0 }));

/* GET_PDOS conn 1 partner source index 0 command */
Cmd get_pdo_c1_partner_src_i0("get_pdo_c1_partner_src_i0",
			      vector<uint8_t>({ 0x10, 0x00, 0x81, 0x00, 0x07 }),
			      vector<uint8_t>({ 0x2c, 0x91, 0x1, 0x2e, 0x2c,
						0xd1, 0x2, 0x0, 0x2c, 0xb1, 0x4,
						0x0, 0x2c, 0x41, 0x6, 0x0 }));

/* GET_PDOS conn 1 partner source index 4 command */
Cmd get_pdo_c1_partner_src_i4("get_pdo_c1_partner_src_i4",
			      vector<uint8_t>({ 0x10, 0x00, 0x81, 0x04,
						0x06 }));

/* GET_ALTERNATE_MODES sop index 0 command */
Cmd get_alt_mode_sop_i0("get_alt_mode_sop_i0",
			vector<uint8_t>({ 0x0c, 0x00, 0x01, 0x01, 0x00,
					  0x00 }));

/* GET_ALTERNATE_MODES sop index 0 values command */
Cmd get_alt_mode_sop_i0_val(
	"get_alt_mode_sop_i0_val",
	vector<uint8_t>({ 0x0c, 0x00, 0x01, 0x01, 0x00, 0x00 }),
	vector<uint8_t>({ 0x1, 0xff, 0x45, 0x0, 0x1c, 0x0 }));

/* GET_ALTERNATE_MODES sop index 1 values command */
Cmd get_alt_mode_sop_i1("get_alt_mode_sop_i1",
			vector<uint8_t>({ 0x0c, 0x00, 0x01, 0x01, 0x01,
					  0x00 }));

/* GET_CURRENT_CAM command */
Cmd get_current_cam_c1("get_current_cam_c1", vector<uint8_t>({ 0xe, 0x0, 0x1 }),
		       vector<uint8_t>({ 0x1 }));

/* GET_PDOS conn 1 partner sink 4 command */
Cmd get_pdo_c1_partner_snk("get_pdo_c1_partner_snk",
			   vector<uint8_t>({ 0x10, 0x00, 0x81, 0x00, 0x03 }),
			   vector<uint8_t>({ 0xa, 0x90, 0x1, 0x3e }));

/* ACK_CC_CI command - command connector change ack */
Cmd ack_cc_ci_conn("ack_cc_ci_conn", vector<uint8_t>({ 0x4, 0x00, 0x1 }));

/* Messages when LPM alert after disconnecting partner from port 1 happens */

/* GET_CONNECTOR_STATUS conn 1 disconnect command */
Cmd get_conn_status_c1_disconn(
	"get_conn_status_c1_disconn", vector<uint8_t>({ 0x12, 0x00, 0x01 }),
	vector<uint8_t>({ 0x0, 0x41, 0x3, 0x40, 0x0, 0x0, 0x0, 0x0, 0x1, 0xc0,
			  0x40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 }));

/* verifies OPM initialization sequence */
TEST_F(OpmUnitTest, opm_initialization)
{
	Cmd::handle_execute_cmd_ptr fptr = &Cmd::handle_execute_cmd;

	{
		InSequence s;
		EXPECT_CALL(*_pdDriverMock, execute_cmd(_, _, _))
			.Times(44)
			.WillOnce(Invoke(&ppm_reset, fptr))
			.WillOnce(Invoke(&set_ncam_c1, fptr))
			.WillOnce(Invoke(&set_ncam_c2, fptr))
			.WillOnce(Invoke(&ppm_reset, fptr))
			.WillOnce(Invoke(&set_ncam_c1, fptr))
			.WillOnce(Invoke(&set_ncam_c2, fptr))
			.WillOnce(Invoke(&set_notificaiton_en_1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_caps, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_conn_caps_c1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c1_src, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c1_snk_i0, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c1_snk_i4, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_conn_c1_i0, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_conn_c1_i1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_conn_c1_i2, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_conn_status_c1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_conn_caps_c2, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c2_src, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c2_snk_i0, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c2_snk_i4, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_c2_i0, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_c2_i1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_c2_i2, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_conn_status_c2, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&set_notificaiton_en_2, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr));
	}

	/* verify port2 and port3 properties */
	verify_port_properties(2);
	verify_port_properties(3);
	/* initialization is completed now ;) */

	this_thread::sleep_for(std::chrono::milliseconds(100));
}

/* verifies handling of partner connected sequence by the OPM */
TEST_F(OpmUnitTest, opm_lpm_conn_1_connect)
{
	Cmd::handle_execute_cmd_ptr fptr = &Cmd::handle_execute_cmd;

	{
		InSequence s;
		EXPECT_CALL(*_pdDriverMock, execute_cmd(_, _, _))
			.WillOnce(Invoke(&ppm_reset, fptr))
			.WillOnce(Invoke(&set_ncam_c1, fptr))
			.WillOnce(Invoke(&set_ncam_c2, fptr))
			.WillOnce(Invoke(&ppm_reset, fptr))
			.WillOnce(Invoke(&set_ncam_c1, fptr))
			.WillOnce(Invoke(&set_ncam_c2, fptr))
			.WillOnce(Invoke(&set_notificaiton_en_1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_caps, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_conn_caps_c1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c1_src, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c1_snk_i0, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c1_snk_i4, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_conn_c1_i0, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_conn_c1_i1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_conn_c1_i2, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_conn_status_c1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_conn_caps_c2, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c2_src, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c2_snk_i0, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c2_snk_i4, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_c2_i0, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_c2_i1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_c2_i2, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_conn_status_c2, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&set_notificaiton_en_2, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr));

		EXPECT_CALL(*_pdDriverMock, execute_cmd(_, _, _))
			.WillOnce(Invoke(&get_conn_status_c1_update1, fptr))
			.WillOnce(Invoke(&get_conn_status_c1_update1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&ack_cc_ci_conn, fptr))
			.WillOnce(Invoke(&get_conn_status_c1_update2, fptr))
			.WillOnce(Invoke(&get_conn_status_c1_update2, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&ack_cc_ci_conn, fptr))
			.WillOnce(Invoke(&get_pdo_c1_partner_src_err, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_sop_i0, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_conn_status_c1_update3, fptr))
			.WillOnce(Invoke(&get_conn_status_c1_update3, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&ack_cc_ci_conn, fptr))
			.WillOnce(Invoke(&get_conn_status_c1_update4, fptr))
			.WillOnce(Invoke(&get_conn_status_c1_update4, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&ack_cc_ci_conn, fptr))
			.WillOnce(Invoke(&get_pdo_c1_partner_src_i0, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c1_partner_src_i4, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_sop_i0_val, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_sop_i1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_current_cam_c1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_current_cam_c1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_conn_status_c1_update5, fptr))
			.WillOnce(Invoke(&get_conn_status_c1_update5, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&ack_cc_ci_conn, fptr))
			.WillOnce(Invoke(&get_current_cam_c1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_conn_caps_c1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_conn_status_c1_update5, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c1_partner_src_i0, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c1_partner_src_i4, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_current_cam_c1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c1_partner_src_i0, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c1_partner_src_i4, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c1_partner_snk, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr));
	}

	/* verify port2 and port3 properties */
	verify_port_properties(2);
	verify_port_properties(3);
	/* initialization is completed now ;) */

	/*
	 * when partner is connected then Realtek triggers a number of
	 * consecutive interrupts, simulate connecting partner to port2
	 */
	ppm_drv->lpm_alert(ppm_drv->dev, 1);
	this_thread::sleep_for(std::chrono::milliseconds(250));
	ppm_drv->lpm_alert(ppm_drv->dev, 1);
	this_thread::sleep_for(std::chrono::milliseconds(250));
	ppm_drv->lpm_alert(ppm_drv->dev, 1);
	this_thread::sleep_for(std::chrono::milliseconds(250));
	ppm_drv->lpm_alert(ppm_drv->dev, 1);
	this_thread::sleep_for(std::chrono::milliseconds(250));
	ppm_drv->lpm_alert(ppm_drv->dev, 1);
	this_thread::sleep_for(std::chrono::milliseconds(500));

	/* verify port2-partner properties */
	verify_port_partner_properties(2);

	this_thread::sleep_for(std::chrono::milliseconds(100));
}

/* verifies handling of partner connected and disconnected sequence by the OPM
 */
TEST_F(OpmUnitTest, opm_lpm_conn_1_connect_and_disconnect)
{
	Cmd::handle_execute_cmd_ptr fptr = &Cmd::handle_execute_cmd;

	{
		InSequence s;
		EXPECT_CALL(*_pdDriverMock, execute_cmd(_, _, _))
			.WillOnce(Invoke(&ppm_reset, fptr))
			.WillOnce(Invoke(&set_ncam_c1, fptr))
			.WillOnce(Invoke(&set_ncam_c2, fptr))
			.WillOnce(Invoke(&ppm_reset, fptr))
			.WillOnce(Invoke(&set_ncam_c1, fptr))
			.WillOnce(Invoke(&set_ncam_c2, fptr))
			.WillOnce(Invoke(&set_notificaiton_en_1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_caps, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_conn_caps_c1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c1_src, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c1_snk_i0, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c1_snk_i4, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_conn_c1_i0, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_conn_c1_i1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_conn_c1_i2, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_conn_status_c1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_conn_caps_c2, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c2_src, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c2_snk_i0, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c2_snk_i4, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_c2_i0, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_c2_i1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_c2_i2, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_conn_status_c2, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&set_notificaiton_en_2, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr));

		EXPECT_CALL(*_pdDriverMock, execute_cmd(_, _, _))
			.WillOnce(Invoke(&get_conn_status_c1_update1, fptr))
			.WillOnce(Invoke(&get_conn_status_c1_update1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&ack_cc_ci_conn, fptr))
			.WillOnce(Invoke(&get_conn_status_c1_update2, fptr))
			.WillOnce(Invoke(&get_conn_status_c1_update2, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&ack_cc_ci_conn, fptr))
			.WillOnce(Invoke(&get_pdo_c1_partner_src_err, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_sop_i0, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_conn_status_c1_update3, fptr))
			.WillOnce(Invoke(&get_conn_status_c1_update3, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&ack_cc_ci_conn, fptr))
			.WillOnce(Invoke(&get_conn_status_c1_update4, fptr))
			.WillOnce(Invoke(&get_conn_status_c1_update4, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&ack_cc_ci_conn, fptr))
			.WillOnce(Invoke(&get_pdo_c1_partner_src_i0, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c1_partner_src_i4, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_sop_i0_val, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_alt_mode_sop_i1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_current_cam_c1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_current_cam_c1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_conn_status_c1_update5, fptr))
			.WillOnce(Invoke(&get_conn_status_c1_update5, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&ack_cc_ci_conn, fptr))
			.WillOnce(Invoke(&get_current_cam_c1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_conn_caps_c1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))

			.WillOnce(Invoke(&get_conn_status_c1_update5, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c1_partner_src_i0, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c1_partner_src_i4, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_current_cam_c1, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c1_partner_src_i0, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c1_partner_src_i4, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&get_pdo_c1_partner_snk, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr));

		EXPECT_CALL(*_pdDriverMock, execute_cmd(_, _, _))
			.WillOnce(Invoke(&get_conn_status_c1_disconn, fptr))
			.WillOnce(Invoke(&get_conn_status_c1_disconn, fptr))
			.WillOnce(Invoke(&ack_cc_ci, fptr))
			.WillOnce(Invoke(&ack_cc_ci_conn, fptr));
	}

	/* verify port2 and port3 properties */
	verify_port_properties(2);
	verify_port_properties(3);
	/* initialization is completed now ;) */

	/*
	 * when partner is connected then Realtek triggers a number of
	 * consecutive interrupts, simulate connecting partner to port2
	 */
	ppm_drv->lpm_alert(ppm_drv->dev, 1);
	this_thread::sleep_for(std::chrono::milliseconds(250));
	ppm_drv->lpm_alert(ppm_drv->dev, 1);
	this_thread::sleep_for(std::chrono::milliseconds(250));
	ppm_drv->lpm_alert(ppm_drv->dev, 1);
	this_thread::sleep_for(std::chrono::milliseconds(250));
	ppm_drv->lpm_alert(ppm_drv->dev, 1);
	this_thread::sleep_for(std::chrono::milliseconds(250));
	ppm_drv->lpm_alert(ppm_drv->dev, 1);
	this_thread::sleep_for(std::chrono::milliseconds(500));

	/* verify port2-partner properties */
	verify_port_partner_properties(2);
	this_thread::sleep_for(std::chrono::milliseconds(500));

	/* simulate parter disconnect from port2 */
	ppm_drv->lpm_alert(ppm_drv->dev, 1);
	this_thread::sleep_for(std::chrono::milliseconds(250));

	/* verify port2 disconnect */
	Checker checker;
	string port_dir("/sys/class/typec/port2-partner");
	checker.addDirToCheck(port_dir, false);
	EXPECT_EQ(0, checker.verify()) << "PORT2-PARTNER DIR STILL EXISTS ";

	this_thread::sleep_for(std::chrono::milliseconds(100));
}
