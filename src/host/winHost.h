#ifndef WHOST_H
#define WHOST_H
#include <cstdint>


class winHost
{
private:
	struct impl;
	impl* m_impl;
public:
	winHost();
	~winHost();

	bool create(void* parentHwnd, uint32_t width, uint32_t height);
	void resize(uint32_t width, uint32_t height);
	void* getHandle() const;

};

#endif // !WHOST_H



