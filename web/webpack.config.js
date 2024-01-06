const path = require("path");
const fs = require("fs");
const glob = require("glob-all");
const fg = require("fast-glob");
const svgToMiniDataURI = require("mini-svg-data-uri");
const webpack = require("webpack");
const WebpackBar = require("webpackbar");
const HtmlWebpackPlugin = require("html-webpack-plugin");
const TerserPlugin = require("terser-webpack-plugin");
const MiniCssExtractPlugin = require("mini-css-extract-plugin");
const CssMinimizerPlugin = require("css-minimizer-webpack-plugin");
const { PurgeCSSPlugin } = require("purgecss-webpack-plugin");
// const ImageMinimizerPlugin = require("image-minimizer-webpack-plugin");
const CompressionPlugin = require("compression-webpack-plugin");
const FileManagerPlugin = require("filemanager-webpack-plugin");
const { BundleAnalyzerPlugin } = require("webpack-bundle-analyzer");

function readConfig() {
    let config = {};
    let str = fs.readFileSync(path.resolve(__dirname, "..", "config.h"), "utf8");
    let lines = str.split("\n");
    for (let line of lines) {
        line = line.trim();
        let match = line.match(/^#define\s+(\w+)\s*"?(.*?)"?\s*(?:\/\/.*|\/\*.*)?$/);
        if (match) {
            if (match[1].startsWith("__"))
                continue;
            config[match[1]] = match[2];
        }
    }
    return config;
}

module.exports = (env) => {
    const mode = env.production ? "production" : "development";
    console.log("Build mode:", mode);
    const config = readConfig();
    console.log("Device config:", JSON.stringify(config, null, 4));
    const deployPath = fg.convertPathToPattern(path.resolve(__dirname, "..", "data", "www"));
    console.log("Deploy webpage to", deployPath);

    return {
        mode: mode,
        entry: "./index.js",
        output: {
            path: path.resolve(__dirname, "build"),
            filename: "bundle.js",
            clean: true
        },
        plugins: [
            new WebpackBar(),
            new webpack.DefinePlugin({
                "process.env": {
                    "NODE_ENV": JSON.stringify(mode)
                },
                "DEVICE_CONFIG": config
            }),
            new HtmlWebpackPlugin({
                title: config.NAME || "RGBLight",
                filename: "index.html",
                template: "./index.html",
                favicon: "./public/favicon.ico",
                minify: true
            }),
            new MiniCssExtractPlugin(),
            new PurgeCSSPlugin({
                paths: glob.sync([
                    path.join(__dirname, "index.html"),
                    path.join(__dirname, "*.css")
                ], { nodir: true }),
                safelist: [/^picker/, /^slide/, /weui-btn_warn/, /weui-icon-success-no-circle/, /weui-icon-warn/]
            }),
            new CompressionPlugin({
                algorithm: "gzip",
                exclude: [
                    /\.map$/
                ]
            }),
            new FileManagerPlugin({
                events: {
                    onEnd: [
                        {
                            delete: [{
                                source: `${deployPath}/**/*`,
                                options: {
                                    force: true
                                }
                            }]
                        },
                        {
                            copy: [
                                { source: "./build/*.gz", destination: `${deployPath}` }
                            ]
                        }
                    ]
                }
            }),
            new BundleAnalyzerPlugin({
                analyzerMode: "disabled",
                generateStatsFile: true
            })
        ],
        module: {
            rules: [
                {
                    test: /\.css$/i,
                    use: [MiniCssExtractPlugin.loader, "css-loader"]
                },
                {
                    test: /\.(png|jpg|jpeg|gif)$/i,
                    type: "asset/resource",
                    generator: {
                        filename: "[name][ext][query]"
                    }
                },
                {
                    test: /\.svg/,
                    type: "asset/inline",
                    generator: {
                        dataUrl: (content) => {
                            return svgToMiniDataURI(content.toString());
                        }
                    }
                }
            ]
        },
        optimization: {
            minimize: true,
            minimizer: [
                new TerserPlugin({
                    terserOptions: {
                        compress: env.production ? {
                            drop_console: ["debug", "trace", "assert"]
                        } : false,
                        mangle: env.production ? {
                            properties: {
                                builtins: false, // FIXME 开了还能再小 1k, 但是跑不起来
                                keep_quoted: true,
                                reserved: ["$dialog", "$toast", "$info"],
                                undeclared: true
                            },
                            module: true,
                            reserved: [],
                            toplevel: true
                        } : false
                    }
                }),
                new CssMinimizerPlugin({
                    minimizerOptions: {
                        preset: [
                            "default",
                            {
                                discardComments: { removeAll: true }
                            }
                        ]
                    }
                })
            ]
        },
        devtool: env.production ? "source-map" : "eval-source-map",
        devServer: {
            static: "./public",
            historyApiFallback: true,
            hot: true,
            client: {
                progress: true
            },
            proxy: [
                {
                    context: ["/version", "/config", "/status", "/list", "/upload", "/download", "/delete", "/upgrade"],
                    target: "http://rgblight/",
                    secure: false
                }
            ]
        }
    };
};
