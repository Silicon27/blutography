document.addEventListener('DOMContentLoaded', async () => {
    console.log('Slideshow initialization started');
    
    const slidesContainer = document.getElementById('slides-container');
    const dotsContainer = document.getElementById('dots-container');
    const cacheName = 'blutography-assets-v5';

    if (!slidesContainer || !dotsContainer) {
        console.error('Required containers not found');
        return;
    }

    let currentSlide = 0;
    let slideInterval;
    let imageObjectURLs = [];
    let imageNames = [];

    async function init() {
        try {
            console.log('Fetching manifest...');
            const response = await fetch('/assets/manifest.json');
            if (!response.ok) throw new Error('Failed to fetch manifest');
            
            imageNames = await response.json();
            console.log('Manifest loaded:', imageNames);

            // Initialize placeholders
            imageNames.forEach((name, index) => {
                const img = document.createElement('img');
                img.alt = `Slide ${index + 1}`;
                if (index === 0) img.classList.add('active');
                slidesContainer.appendChild(img);

                const dot = document.createElement('div');
                dot.classList.add('dot');
                if (index === 0) dot.classList.add('active');
                dot.addEventListener('click', () => {
                    goToSlide(index, true);
                });
                dotsContainer.appendChild(dot);
            });

            // Start sequential loading
            loadImagesSequentially();
            
            // Start cycle
            startInterval();
        } catch (error) {
            slidesContainer.innerHTML = `<div style="color: white; padding: 20px;">Failed to initialize slideshow. <br><small>Error: ${error.message}</small></div>`;
            console.error('Init failed:', error);
        }
    }

    async function loadImagesSequentially() {
        const cache = await caches.open(cacheName);
        const imgElements = slidesContainer.querySelectorAll('img');

        for (let i = 0; i < imageNames.length; i++) {
            const name = imageNames[i];
            const url = `/assets/${name}`;
            
            try {
                let response = await cache.match(url);
                if (!response) {
                    console.log(`Fetching ${name} (Smallest first loading)...`);
                    response = await fetch(url);
                    if (!response.ok) throw new Error(`HTTP ${response.status}`);
                    await cache.put(url, response.clone());
                } else {
                    console.log(`${name} loaded from cache`);
                }

                const blob = await response.blob();
                const objectUrl = URL.createObjectURL(blob);
                imageObjectURLs[i] = objectUrl;

                imgElements[i].onload = () => {
                    console.log(`Image rendered: ${name}`);
                };
                imgElements[i].onerror = () => {
                    console.error(`Render failed: ${name}`);
                };

                imgElements[i].src = objectUrl;
                
                // If it's the first image, it's already active and now has a src
            } catch (e) {
                console.error(`Failed to load ${name}:`, e);
            }
        }
    }

    function goToSlide(index, instant = false) {
        if (index === currentSlide) return;
        
        const imgs = slidesContainer.querySelectorAll('img');
        const dots = dotsContainer.querySelectorAll('.dot');

        if (instant) {
            slidesContainer.classList.add('instant');
        }

        if (imgs[currentSlide]) imgs[currentSlide].classList.remove('active');
        if (dots[currentSlide]) dots[currentSlide].classList.remove('active');

        currentSlide = index;

        if (imgs[currentSlide]) imgs[currentSlide].classList.add('active');
        if (dots[currentSlide]) dots[currentSlide].classList.add('active');

        if (instant) {
            void slidesContainer.offsetWidth;
            slidesContainer.classList.remove('instant');
        }

        resetInterval();
    }

    function nextSlide() {
        if (imageNames.length === 0) return;
        const next = (currentSlide + 1) % imageNames.length;
        goToSlide(next);
    }

    function startInterval() {
        if (slideInterval) return;
        slideInterval = setInterval(nextSlide, 5000);
    }

    function resetInterval() {
        clearInterval(slideInterval);
        slideInterval = null;
        startInterval();
    }

    init();
});