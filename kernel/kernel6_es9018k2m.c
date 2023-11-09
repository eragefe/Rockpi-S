/*
 * ES9018-K2M based Audio DAC support
 *
 * Christian Kroener <ckroener@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/of_platform.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include "es9018k2m.h"

/* External module: include config */
#include <generated/autoconf.h>

#define ES9018K2M_CODEC_NAME "es9018k2m-codec"
#define ES9018K2M_CODEC_DAI_NAME "es9018k2m-hifi"

/* Not writeable: 64 66 67 68 69 */

static const struct reg_default es9018k2m_reg_defaults[] = {
	{ ES9018K2M_SYSTEM_SETTING,	0x00 },
	{ ES9018K2M_INPUT_CONFIG,	0x8c }, /* Default: auto detect I2S, DSD or SPDIF (on DATA_CLK) input */
	{ ES9018K2M_AUTOMUTE_TIME,	0x00 },
	{ ES9018K2M_AUTOMUTE_LEVEL,	0x68 },
	{ ES9018K2M_DEEMPHASIS,		0x42 },
	{ ES9018K2M_GENERAL_SET,	0x80 },
	{ ES9018K2M_GPIO_CONFIG,	0x81 }, /* GPIO1 is set to DPLL lock, GPIO2 set to input (for SPDIF) */
	{ ES9018K2M_W_MODE_CONTROL,	0x00 },
	{ ES9018K2M_V_MODE_CONTROL,	0x05 },
	{ ES9018K2M_CHANNEL_MAP,	0x02 }, /* Stereo, 0x00 is mono left, 0x03 mono right */
	{ ES9018K2M_DPLL,		0x5a }, /* Upper 4 bits for I2S DPLL setting, lower 4 bits for DSD */
	{ ES9018K2M_THD_COMPENSATION,	0x40 },
	{ ES9018K2M_SOFT_START,		0x8a },
	{ ES9018K2M_VOLUME1,		0x00 },
	{ ES9018K2M_VOLUME2,		0x00 },
	{ ES9018K2M_MASTERTRIM0,	0xff },
	{ ES9018K2M_MASTERTRIM1,	0xff },
	{ ES9018K2M_MASTERTRIM2,	0xff },
	{ ES9018K2M_MASTERTRIM3,	0x7f },
	{ ES9018K2M_INPUT_SELECT,	0x00 },
	{ ES9018K2M_2_HARMONIC_COMPENSATION_0,	0x00 },
	{ ES9018K2M_2_HARMONIC_COMPENSATION_1,	0x00 },
	{ ES9018K2M_3_HARMONIC_COMPENSATION_0,	0x00 },
	{ ES9018K2M_3_HARMONIC_COMPENSATION_1,	0x00 },
	{ ES9018K2M_program_FIR_ADDR,	0x00 },
	{ ES9018K2M_program_FIR_DATA1,	0x00 },
	{ ES9018K2M_program_FIR_DATA2,	0x00 },
	{ ES9018K2M_program_FIR_DATAC,	0x00 },
	{ ES9018K2M_program_FIR_CONTROL,	0x00 },
};

static bool es9018k2m_readable_reg(struct device *dev, unsigned int reg)
{
	if(reg <= ES9018K2M_CACHEREGNUM && reg != 2 && reg !=3)
		return 1;
	else if(65 <= reg && reg <= 69)
		return 1;
	else if(70 <= reg && reg <= 93)
		return 1;
	else
		return 0;
}

static bool es9018k2m_writeable_reg(struct device *dev, unsigned int reg)
{
	if(reg > ES9018K2M_CACHEREGNUM)
		return  0;
	else if(reg == 2 || reg == 3)
		return 0;
	else
		return 1;
}
/*
static bool es9018k2m_volatile_reg(struct device *dev, unsigned int reg)
{
	return false;
}
*/
struct es9018k2m_priv {
    struct regmap *regmap;
    unsigned int fmt;
};

#define ES9018K2M_FORMATS (\
            SNDRV_PCM_FMTBIT_S16_LE | \
            SNDRV_PCM_FMTBIT_S24_3LE | \
            SNDRV_PCM_FMTBIT_S24_LE | \
            SNDRV_PCM_FMTBIT_S32_LE | \
            SNDRV_PCM_FMTBIT_DSD_U32_LE | \
            0)


static const char * const es9018k2m_dpll_texts[] = {
	"OFF",
	"LOW",
	"2x",
	"3x",
	"4x",
	"5x",
	"6x",
	"7x",
	"8x",
	"9x",
	"10x",
	"11x",
	"12x",
	"13x",
	"14x",
	"HI",
};

static SOC_ENUM_SINGLE_DECL(es9018k2m_dpll_i2s, ES9018K2M_DPLL, 4, es9018k2m_dpll_texts);
static SOC_ENUM_SINGLE_DECL(es9018k2m_dpll_dsd, ES9018K2M_DPLL, 0, es9018k2m_dpll_texts);

static const char * const es9018k2m_deemph_texts[] = {
	"OFF",
	"Auto",
	"32k",
	"44k",
	"48k",
};

static const unsigned int es9018k2m_deemph_values[] = {
	0x4a,
	0x8a,
	0x0a,
	0x1a,
	0x2a,
};

static SOC_VALUE_ENUM_SINGLE_DECL(es9018k2m_deemph, ES9018K2M_DEEMPHASIS, 0, 0xFF, es9018k2m_deemph_texts, es9018k2m_deemph_values);

static const char * const es9018k2m_chmap_texts[] = {
	"Stereo",
	"Mono L",
	"Mono R",
};

static const unsigned int es9018k2m_chmap_values[] = {
	0x02,
	0x00,
	0x03,
};

static SOC_VALUE_ENUM_SINGLE_DECL(es9018k2m_chmap, ES9018K2M_CHANNEL_MAP, 0, 0x03, es9018k2m_chmap_texts, es9018k2m_chmap_values);

static const char * const es9018k2m_iirbw_texts[] = {
	"47k",
	"50k",
	"60k",
	"70k",
};

static SOC_ENUM_SINGLE_DECL(es9018k2m_iirbw, ES9018K2M_GENERAL_SET, 2, es9018k2m_iirbw_texts);

static const char * const es9018k2m_fir_texts[] = {
	"Fast",
	"Slow",
	"MinPh",
};

static SOC_ENUM_SINGLE_DECL(es9018k2m_fir, ES9018K2M_GENERAL_SET, 5, es9018k2m_fir_texts);

static const char * const es9018k2m_onoff_texts[] = {
	"On",
	"Off",
};

static SOC_ENUM_SINGLE_DECL(es9018k2m_osf, ES9018K2M_INPUT_SELECT, 0, es9018k2m_onoff_texts);

static SOC_ENUM_SINGLE_DECL(es9018k2m_iir, ES9018K2M_INPUT_SELECT, 2, es9018k2m_onoff_texts);

static const DECLARE_TLV_DB_SCALE(es9018k2m_dac_tlv, -12750, 50, 1);

static const struct snd_kcontrol_new es9018k2m_codec_controls[] = {
    SOC_DOUBLE_R_TLV("Master Playback Volume", ES9018K2M_VOLUME1, ES9018K2M_VOLUME2, 0, 0xFF, 1, es9018k2m_dac_tlv),
    SOC_ENUM("Deemph", es9018k2m_deemph),
    SOC_ENUM("I2S DPLL", es9018k2m_dpll_i2s),
    SOC_ENUM("DSD DPLL", es9018k2m_dpll_dsd),
    SOC_ENUM("CH Map", es9018k2m_chmap),
    SOC_ENUM("IIR BW", es9018k2m_iirbw),
    SOC_ENUM("Use IIR", es9018k2m_iir),
    SOC_ENUM("FIR", es9018k2m_fir),
    SOC_ENUM("Use OSF", es9018k2m_osf),
};

const struct regmap_config es9018k2m_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 93,
	.reg_defaults = es9018k2m_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(es9018k2m_reg_defaults),
	.writeable_reg = es9018k2m_writeable_reg,
	.readable_reg = es9018k2m_readable_reg,
	.cache_type = REGCACHE_RBTREE,
};
EXPORT_SYMBOL_GPL(es9018k2m_regmap);
/*
static int es9018k2m_hw_params(
	struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct es9018k2m_priv *es9018k2m = snd_soc_component_get_drvdata(component);

	unsigned int iface;
	
	regmap_read(es9018k2m->regmap, ES9018K2M_INPUT_CONFIG, &iface);
	
	iface &= 0x3f;

	switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S16_LE:
			iface |= 0x0;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			iface |= 0x40;
			break;
		case SNDRV_PCM_FORMAT_S32_LE:
			iface |= 0x80;
			break;
		default:
			return 0;
	}

	regmap_write(es9018k2m->regmap, ES9018K2M_INPUT_CONFIG, iface);

	return 0;
}

static const struct snd_soc_dai_ops es9018k2m_dai_ops = {
	.hw_params	= es9018k2m_hw_params,
};
*/
static struct snd_soc_dai_driver es9018k2m_dai = {
    .name = ES9018K2M_CODEC_DAI_NAME,
    .playback = {
	.stream_name = "Playback",
        .channels_min = 1,
        .channels_max = 2,
        .rate_min = 22050,
        .rate_max = 384000,
        .rates = SNDRV_PCM_RATE_KNOT,
        .formats = ES9018K2M_FORMATS,
    },
/*    .ops = &es9018k2m_dai_ops,*/
};

static struct snd_soc_component_driver es9018k2m_component_driver = {
    .controls = es9018k2m_codec_controls,
    .num_controls = ARRAY_SIZE(es9018k2m_codec_controls),
    .idle_bias_on = 1,
    .use_pmdown_time = 1,
    .endianness = 1,
    .legacy_dai_naming = 1,
};

int es9018k2m_probe(struct device *dev, struct regmap *regmap)
{
	struct es9018k2m_priv *es9018k2m;
	int ret = 0;

	es9018k2m = devm_kzalloc(dev, sizeof(struct es9018k2m_priv), GFP_KERNEL);
	if (!es9018k2m)
		return -ENOMEM;
	
	dev_set_drvdata(dev, es9018k2m);
	es9018k2m->regmap = regmap;
	ret = devm_snd_soc_register_component(dev, &es9018k2m_component_driver,
				    &es9018k2m_dai, 1);
	if (ret != 0) {
		dev_err(dev, "Failed to register CODEC: %d\n", ret);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(es9018k2m_probe);

#if defined(CONFIG_OF)
static const struct of_device_id es9018k2m_dt_ids[] = {
    { .compatible = "ess,es9018k2m", },
    { }
};

MODULE_DEVICE_TABLE(of, es9018k2m_dt_ids);
#endif

static const struct i2c_device_id es9018k2m_i2c_id[] = {
	{ "es9018k2m", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, es9018k2m_i2c_id);

static int es9018k2m_i2c_probe(struct i2c_client *i2c,
			      const struct i2c_device_id *id)
{
	int ret;
	struct regmap *regmap;
	struct regmap_config config = es9018k2m_regmap;

	regmap = devm_regmap_init_i2c(i2c, &config);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err(&i2c->dev, "Failed to create regmap: %d\n", ret);
		return ret;
	}

	return es9018k2m_probe(&i2c->dev, regmap);
}

static struct i2c_driver es9018k2m_i2c_driver = {
	.driver = {
		.name	= "es9018k2m",
		.of_match_table = of_match_ptr(es9018k2m_dt_ids),
	},
	.id_table	= es9018k2m_i2c_id,
	.probe		= es9018k2m_i2c_probe,
};

module_i2c_driver(es9018k2m_i2c_driver);

MODULE_AUTHOR("Christian Kroener");
MODULE_DESCRIPTION("ESS Technology ES9018-K2M Audio DAC");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:asoc-es9018k2m-codec");
