import stylelint from 'stylelint';

const {
    createPlugin,
    utils: { report, ruleMessages, validateOptions },
} = stylelint;

const lineLengthRule = 'flipkiln/max-line-length';
const messages = ruleMessages(lineLengthRule, {
    expected: (limit, length) => `Expected at most ${limit} characters, found ${length}`,
});

const maxLineLength = createPlugin(lineLengthRule, (limit) => (root, result) => {
    const valid = validateOptions(result, lineLengthRule, {
        actual: limit,
        possible: (value) => Number.isInteger(value) && value > 0,
    });
    if (!valid) return;

    const lines = root.source.input.css.split(/\r?\n/);
    for (const [index, line] of lines.entries()) {
        const characters = Array.from(line);
        if (characters.length <= limit) continue;

        const lineNumber = index + 1;
        report({
            result,
            ruleName: lineLengthRule,
            message: messages.expected(limit, characters.length),
            node: root,
            start: { line: lineNumber, column: characters.slice(0, limit).join('').length + 1 },
            end: { line: lineNumber, column: line.length + 1 },
        });
    }
});

export default {
    customSyntax: 'postcss-scss',
    plugins: ['@stylistic/stylelint-plugin', 'stylelint-scss', maxLineLength],
    rules: {
        '@stylistic/scss/indentation': 4,
        [lineLengthRule]: 110,
        'scss/at-rule-no-unknown': true,
        'scss/no-duplicate-dollar-variables': true,
    },
};
