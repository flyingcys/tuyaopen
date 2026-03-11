### PR 描述/PR description
[在此详细描述 PR 的内容]/[Describe the PR content in detail here]


### 代码质量/Code Quality:
在本次拉取请求中，我已考虑以下事项 As part of this pull request, I've considered the following:
- [ ] 确保代码注释和文档清晰，并使用英文注释以保证代码可读性。Ensure that the code comments and documentation are clear, and use English for comments to ensure code readability.
- [ ] 确保文件头遵循[文件头格式](https://www.tuyaopen.ai/zh/docs/contribute/coding-style-guide#%E5%A4%B4%E6%96%87%E4%BB%B6)。Ensure that the file header follows the [File Header Format](https://www.tuyaopen.ai/docs/contribute/coding-style-guide#file-header-format).
- [ ] 确保函数头遵循 [Doxygen 格式](https://www.tuyaopen.ai/zh/docs/contribute/coding-style-guide#%E6%B3%A8%E9%87%8A)。Ensure that function headers follow the Doxygen format as specified in [Comments](https://www.tuyaopen.ai/docs/contribute/coding-style-guide#comments).
- [ ] 已查阅 [编码风格指南](https://www.tuyaopen.ai/zh/docs/contribute/coding-style-guide)，并核查代码风格合规性，包括缩进、空格、命名规范及其他风格要求。 Reviewed the [Coding Style Guide](https://www.tuyaopen.ai/docs/contribute/coding-style-guide) and verified code style compliance, including indentation, spacing, naming conventions, and other style guidelines.
- [ ] 已使用[代码格式化工具](https://www.tuyaopen.ai/zh/docs/contribute/coding-style-guide#%E6%A0%BC%E5%BC%8F%E5%8C%96%E4%BB%A3%E7%A0%81)确保符合 TuyaOpen 编码规范。Have used the [code-formatting](https://www.tuyaopen.ai/docs/contribute/coding-style-guide#code-formatting) source code formatting tool to ensure compliance with TuyaOpen coding standards.

### 测试验证/Testing:
- [ ] 涉及源码改动时，已运行最小相关验证命令，并在 PR 描述中说明结果。When source files changed, I ran the smallest relevant verification command and summarized the result in the PR description.
- [ ] 若改动覆盖 Host 单元测试范围，已运行 `bash tools/test/run_host_tests.sh`。If the change touched Host unit test coverage, I ran `bash tools/test/run_host_tests.sh`.
- [ ] 若改动引入或调整 Target 单测骨架，已运行 `python -m pytest tests/target/pytest -m target -v` 或说明跳过原因。If the change added or updated target test skeleton code, I ran `python -m pytest tests/target/pytest -m target -v` or documented why it was skipped.
- [ ] 若本次改动没有新增或更新单元测试，已在 PR 描述中说明原因。If this change did not add or update unit tests, I documented the reason in the PR description.
